#include "esp_log.h"
#include "esp_netif.h"
#ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif
#include "esp_websocket_client.h"
#include "esp_tls.h"
#include "esp_timer.h"

#include "protocol.h"
#include "signaling.h"
#include "url.h"
#include "utils.h"

static const char *TAG = "livekit_signaling";

#define SIGNAL_WS_BUFFER_SIZE          20 * 1024
#define SIGNAL_WS_RECONNECT_TIMEOUT_MS 1000
#define SIGNAL_WS_NETWORK_TIMEOUT_MS   10000
#define SIGNAL_WS_CLOSE_CODE           1000
#define SIGNAL_WS_CLOSE_TIMEOUT_MS     250

typedef struct {
    esp_websocket_client_handle_t ws;
    signal_options_t         options;
    esp_timer_handle_t       ping_timer;
    bool                     last_attempt_failed;

    int32_t ping_interval_ms;
    int32_t ping_timeout_ms;
    int64_t rtt;
} signal_t;

static inline void state_changed(signal_t *sg, signal_state_t state)
{
    sg->options.on_state_changed(state, sg->options.ctx);
}

static inline signal_state_t failed_state_from_http_status(int status)
{
    switch (status) {
        case 400: return SIGNAL_STATE_FAILED_BAD_TOKEN;
        case 401: return SIGNAL_STATE_FAILED_UNAUTHORIZED;
        default:  return status > 400 && status < 500 ?
                    SIGNAL_STATE_FAILED_CLIENT_OTHER :
                    SIGNAL_STATE_FAILED_INTERNAL;
    }
}

static signal_err_t send_request(signal_t *sg, livekit_pb_signal_request_t *request)
{
    // TODO: Optimize (use static buffer for small messages)
    ESP_LOGD(TAG, "Sending request: type=%d", request->which_message);

    size_t encoded_size = 0;
    if (!pb_get_encoded_size(&encoded_size, LIVEKIT_PB_SIGNAL_REQUEST_FIELDS, request)) {
        return SIGNAL_ERR_MESSAGE;
    }
    uint8_t *enc_buf = (uint8_t *)malloc(encoded_size);
    if (enc_buf == NULL) {
        return SIGNAL_ERR_NO_MEM;
    }
    int ret = SIGNAL_ERR_NONE;
    do {
        pb_ostream_t stream = pb_ostream_from_buffer(enc_buf, encoded_size);
        if (!pb_encode(&stream, LIVEKIT_PB_SIGNAL_REQUEST_FIELDS, request)) {
            ESP_LOGE(TAG, "Failed to encode request");
            ret = SIGNAL_ERR_MESSAGE;
            break;
        }
        if (esp_websocket_client_send_bin(sg->ws,
                (const char *)enc_buf,
                stream.bytes_written,
                portMAX_DELAY) < 0) {
            ESP_LOGE(TAG, "Failed to send request");
            ret = SIGNAL_ERR_MESSAGE;
            break;
        }
    } while (0);
    free(enc_buf);
    return ret;
}

static void send_ping(void *arg)
{
    signal_t *sg = (signal_t *)arg;

    livekit_pb_signal_request_t req = LIVEKIT_PB_SIGNAL_REQUEST_INIT_DEFAULT;
    req.which_message = LIVEKIT_PB_SIGNAL_REQUEST_PING_REQ_TAG;
    req.message.ping_req.timestamp = get_unix_time_ms();
    req.message.ping_req.rtt = sg->rtt;

    send_request(sg, &req);
}

/// Processes responses before forwarding them to the receiver.
static inline bool res_middleware(signal_t *sg, livekit_pb_signal_response_t *res)
{
    if (res->which_message != LIVEKIT_PB_SIGNAL_RESPONSE_PONG_RESP_TAG &&
        res->which_message != LIVEKIT_PB_SIGNAL_RESPONSE_JOIN_TAG) {
        return true;
    }
    bool should_forward = false;
    switch (res->which_message) {
        case LIVEKIT_PB_SIGNAL_RESPONSE_PONG_RESP_TAG:
            livekit_pb_pong_t *pong = &res->message.pong_resp;
            sg->rtt = get_unix_time_ms() - pong->last_ping_timestamp;
            // TODO: Reset ping timeout
            should_forward = false;
            break;
        case LIVEKIT_PB_SIGNAL_RESPONSE_JOIN_TAG:
            livekit_pb_join_response_t *join = &res->message.join;
            sg->ping_interval_ms = join->ping_interval * 1000;
            sg->ping_timeout_ms = join->ping_timeout * 1000;
            esp_timer_start_periodic(sg->ping_timer, sg->ping_interval_ms * 1000);
            should_forward = true;
            break;
        default:
            should_forward = false;
    }
    return should_forward;
}

static void on_ws_event(void *ctx, esp_event_base_t base, int32_t event_id, void *event_data)
{
    assert(ctx != NULL);
    signal_t *sg = (signal_t *)ctx;
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

    switch (event_id) {
        case WEBSOCKET_EVENT_BEFORE_CONNECT:
            sg->last_attempt_failed = false;
            state_changed(sg, SIGNAL_STATE_CONNECTING);
            break;
        case WEBSOCKET_EVENT_CLOSED:
        case WEBSOCKET_EVENT_DISCONNECTED:
            esp_timer_stop(sg->ping_timer);
            if (!sg->last_attempt_failed) {
                state_changed(sg, SIGNAL_STATE_DISCONNECTED);
            }
            break;
        case WEBSOCKET_EVENT_ERROR:
            int http_status = data->error_handle.esp_ws_handshake_status_code;
            signal_state_t state = http_status != 0 ?
                failed_state_from_http_status(http_status) :
                SIGNAL_STATE_FAILED_UNREACHABLE;
            state_changed(sg, state);
            break;
        case WEBSOCKET_EVENT_CONNECTED:
            state_changed(sg, SIGNAL_STATE_CONNECTED);
            break;
        case WEBSOCKET_EVENT_DATA:
            if (data->op_code != WS_TRANSPORT_OPCODES_BINARY) {
                break;
            }
            if (data->data_len < 1) break;
            livekit_pb_signal_response_t res = {};
            if (!protocol_signal_res_decode((const uint8_t *)data->data_ptr, data->data_len, &res)) {
                break;
            }
            if (!res_middleware(sg, &res)) {
                // Don't forward.
                protocol_signal_res_free(&res);
                break;
            }
            if (!sg->options.on_res(&res, sg->options.ctx)) {
                // Ownership was not taken.
                protocol_signal_res_free(&res);
            }
            break;
        default:
            break;
    }
}

signal_err_t signal_create(signal_handle_t *handle, signal_options_t *options)
{
    if (options == NULL || handle == NULL) {
        return SIGNAL_ERR_INVALID_ARG;
    }

    if (options->on_state_changed == NULL ||
        options->on_res == NULL) {
        ESP_LOGE(TAG, "Missing required event handlers");
        return SIGNAL_ERR_INVALID_ARG;
    }

    signal_t *sg = calloc(1, sizeof(signal_t));
    if (sg == NULL) {
        return SIGNAL_ERR_NO_MEM;
    }
    sg->options = *options;

    esp_timer_create_args_t timer_args = {
        .callback = send_ping,
        .arg = sg,
        .name = "ping"
    };
    if (esp_timer_create(&timer_args, &sg->ping_timer) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create ping timer");
        free(sg);
        return SIGNAL_ERR_OTHER;
    }

    // URL will be set on connect
    static esp_websocket_client_config_t ws_config = {
        .buffer_size = SIGNAL_WS_BUFFER_SIZE,
        .disable_pingpong_discon = true,
        .network_timeout_ms = SIGNAL_WS_NETWORK_TIMEOUT_MS,
        .disable_auto_reconnect = true,
#ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
        .crt_bundle_attach = esp_crt_bundle_attach
#endif
    };
    sg->ws = esp_websocket_client_init(&ws_config);
    if (sg->ws == NULL) {
        ESP_LOGE(TAG, "Failed to initialize WebSocket client");
        esp_timer_delete(sg->ping_timer);
        free(sg);
        return SIGNAL_ERR_WEBSOCKET;
    }
    esp_websocket_register_events(
        sg->ws,
        WEBSOCKET_EVENT_ANY,
        on_ws_event,
        (void *)sg
    );
    *handle = sg;
    return SIGNAL_ERR_NONE;
}

signal_err_t signal_destroy(signal_handle_t handle)
{
    if (handle == NULL) {
        return SIGNAL_ERR_INVALID_ARG;
    }
    signal_t *sg = (signal_t *)handle;
    esp_timer_delete(sg->ping_timer);
    signal_close(handle);
    esp_websocket_client_destroy(sg->ws);
    free(sg);
    return SIGNAL_ERR_NONE;
}

signal_err_t signal_connect(signal_handle_t handle, const char* server_url, const char* token)
{
    if (server_url == NULL || token == NULL || handle == NULL) {
        return SIGNAL_ERR_INVALID_ARG;
    }
    signal_t *sg = (signal_t *)handle;

    char* url = NULL;
    url_build_options options = {
        .server_url = server_url,
        .token = token
    };
    if (!url_build(&options, &url)) {
        return SIGNAL_ERR_INVALID_URL;
    }
    esp_websocket_client_set_uri(sg->ws, url);
    free(url);

    if (esp_websocket_client_start(sg->ws) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WebSocket");
        return SIGNAL_ERR_WEBSOCKET;
    }
    return SIGNAL_ERR_NONE;
}

signal_err_t signal_close(signal_handle_t handle)
{
    if (handle == NULL) {
        return SIGNAL_ERR_INVALID_ARG;
    }
    signal_t *sg = (signal_t *)handle;

    esp_timer_stop(sg->ping_timer);
    if (esp_websocket_client_is_connected(sg->ws) &&
        esp_websocket_client_close(sg->ws, pdMS_TO_TICKS(SIGNAL_WS_CLOSE_TIMEOUT_MS)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to close WebSocket");
        return SIGNAL_ERR_WEBSOCKET;
    }
    return SIGNAL_ERR_NONE;
}

signal_err_t signal_send_leave(signal_handle_t handle)
{
    if (handle == NULL) {
        return SIGNAL_ERR_INVALID_ARG;
    }
    signal_t *sg = (signal_t *)handle;
    livekit_pb_signal_request_t req = LIVEKIT_PB_SIGNAL_REQUEST_INIT_ZERO;
    req.which_message = LIVEKIT_PB_SIGNAL_REQUEST_LEAVE_TAG;

    livekit_pb_leave_request_t leave = {
        .reason = LIVEKIT_PB_DISCONNECT_REASON_CLIENT_INITIATED,
        .action = LIVEKIT_PB_LEAVE_REQUEST_ACTION_DISCONNECT
    };
    req.message.leave = leave;
    return send_request(sg, &req);
}

signal_err_t signal_send_answer(signal_handle_t handle, const char *sdp)
{
    if (sdp == NULL || handle == NULL) {
        return SIGNAL_ERR_INVALID_ARG;
    }
    signal_t *sg = (signal_t *)handle;
    livekit_pb_signal_request_t req = LIVEKIT_PB_SIGNAL_REQUEST_INIT_ZERO;

    livekit_pb_session_description_t desc = {
        .type = "answer",
        .sdp = (char *)sdp
    };
    req.which_message = LIVEKIT_PB_SIGNAL_REQUEST_ANSWER_TAG;
    req.message.answer = desc;
    return send_request(sg, &req);
}

signal_err_t signal_send_offer(signal_handle_t handle, const char *sdp)
{
    if (sdp == NULL || handle == NULL) {
        return SIGNAL_ERR_INVALID_ARG;
    }
    signal_t *sg = (signal_t *)handle;
    livekit_pb_signal_request_t req = LIVEKIT_PB_SIGNAL_REQUEST_INIT_ZERO;

    livekit_pb_session_description_t desc = {
        .type = "offer",
        .sdp = (char *)sdp
    };
    req.which_message = LIVEKIT_PB_SIGNAL_REQUEST_OFFER_TAG;
    req.message.offer = desc;
    return send_request(sg, &req);
}

signal_err_t signal_send_add_track(signal_handle_t handle, livekit_pb_add_track_request_t *add_track_req)
{
    if (handle == NULL || add_track_req == NULL) {
        return SIGNAL_ERR_INVALID_ARG;
    }
    signal_t *sg = (signal_t *)handle;
    livekit_pb_signal_request_t req = LIVEKIT_PB_SIGNAL_REQUEST_INIT_ZERO;
    req.which_message = LIVEKIT_PB_SIGNAL_REQUEST_ADD_TRACK_TAG;
    req.message.add_track = *add_track_req;
    return send_request(sg, &req);
}

signal_err_t signal_send_update_subscription(signal_handle_t handle, const char *sid, bool subscribe)
{
    if (sid == NULL || handle == NULL) {
        return SIGNAL_ERR_INVALID_ARG;
    }
    signal_t *sg = (signal_t *)handle;
    livekit_pb_signal_request_t req = LIVEKIT_PB_SIGNAL_REQUEST_INIT_ZERO;

    livekit_pb_update_subscription_t subscription = {
        .track_sids = (char*[]){(char*)sid},
        .track_sids_count = 1,
        .subscribe = subscribe
    };
    req.which_message = LIVEKIT_PB_SIGNAL_REQUEST_SUBSCRIPTION_TAG;
    req.message.subscription = subscription;
    return send_request(sg, &req);
}