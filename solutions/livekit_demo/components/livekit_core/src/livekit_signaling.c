#include <inttypes.h>
#include <cJSON.h>
#include "esp_log.h"
#include "esp_peer_signaling.h"
#include "esp_netif.h"
#include "media_lib_os.h"
#ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif
#include "esp_websocket_client.h"
#include "esp_tls.h"
#include "esp_timer.h"

#include <livekit_protocol.h>
#include "livekit_signaling.h"
#include "livekit_url.h"
#include "livekit_utils.h"

static const char *TAG = "livekit_signaling";

#define LIVEKIT_SIG_WS_BUFFER_SIZE          2048
#define LIVEKIT_SIG_WS_SEND_TIMEOUT_MS      5000
#define LIVEKIT_SIG_WS_RECONNECT_TIMEOUT_MS 1000
#define LIVEKIT_SIG_WS_NETWORK_TIMEOUT_MS   1000
#define LIVEKIT_SIG_WS_CLOSE_CODE           1000
#define LIVEKIT_SIG_WS_CLOSE_TIMEOUT_MS     250

typedef struct {
    char* url;
    esp_websocket_client_handle_t ws;
    livekit_sig_options_t         options;
    esp_timer_handle_t            ping_timer;

    int32_t                       ping_interval_ms;
    int32_t                       ping_timeout_ms;
    int64_t                       rtt;
} livekit_sig_t;

static esp_websocket_client_config_t default_ws_cfg = {
    .buffer_size = LIVEKIT_SIG_WS_BUFFER_SIZE,
    .disable_pingpong_discon = true,
    .reconnect_timeout_ms = LIVEKIT_SIG_WS_RECONNECT_TIMEOUT_MS,
    .network_timeout_ms = LIVEKIT_SIG_WS_NETWORK_TIMEOUT_MS,
#ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
    .crt_bundle_attach = esp_crt_bundle_attach
#endif
};

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static livekit_sig_err_t send_request(livekit_sig_t *sg, livekit_pb_signal_request_t *request)
{
    // TODO: Optimize (use static buffer for small messages)
    ESP_LOGI(TAG, "Sending request: type=%d", request->which_message);

    size_t encoded_size = 0;
    if (!pb_get_encoded_size(&encoded_size, LIVEKIT_PB_SIGNAL_REQUEST_FIELDS, request)) {
        return LIVEKIT_SIG_ERR_MESSAGE;
    }
    uint8_t *enc_buf = (uint8_t *)malloc(encoded_size);
    if (enc_buf == NULL) {
        return LIVEKIT_SIG_ERR_NO_MEM;
    }
    int ret = LIVEKIT_SIG_ERR_NONE;
    do {
        pb_ostream_t stream = pb_ostream_from_buffer(enc_buf, encoded_size);
        if (!pb_encode(&stream, LIVEKIT_PB_SIGNAL_REQUEST_FIELDS, request)) {
            ESP_LOGE(TAG, "Failed to encode request");
            ret = LIVEKIT_SIG_ERR_MESSAGE;
            break;
        }
        if (esp_websocket_client_send_bin(sg->ws,
                (const char *)enc_buf,
                stream.bytes_written,
                pdMS_TO_TICKS(LIVEKIT_SIG_WS_SEND_TIMEOUT_MS)) < 0) {
            ESP_LOGE(TAG, "Failed to send request");
            ret = LIVEKIT_SIG_ERR_MESSAGE;
            break;
        }
    } while (0);
    free(enc_buf);
    return ret;
}

static void send_ping(void *arg)
{
    livekit_sig_t *sg = (livekit_sig_t *)arg;

    livekit_pb_signal_request_t req = LIVEKIT_PB_SIGNAL_REQUEST_INIT_DEFAULT;
    req.which_message = LIVEKIT_PB_SIGNAL_REQUEST_PING_REQ_TAG;
    req.message.ping_req.timestamp = get_unix_time_ms();
    req.message.ping_req.rtt = sg->rtt;

    send_request(sg, &req);
}

static void handle_res(livekit_sig_t *sg, livekit_pb_signal_response_t *res)
{
    switch (res->which_message) {
        case LIVEKIT_PB_SIGNAL_RESPONSE_PONG_RESP_TAG:
            livekit_pb_pong_t *pong = &res->message.pong_resp;
            sg->rtt = get_unix_time_ms() - pong->last_ping_timestamp;
            // TODO: Reset ping timeout
            break;
        case LIVEKIT_PB_SIGNAL_RESPONSE_REFRESH_TOKEN_TAG:
            // TODO: Handle refresh token
            break;
        case LIVEKIT_PB_SIGNAL_RESPONSE_JOIN_TAG:
            livekit_pb_join_response_t *join_res = &res->message.join;
            sg->ping_interval_ms = join_res->ping_interval * 1000;
            sg->ping_timeout_ms = join_res->ping_timeout * 1000;
            ESP_LOGI(TAG, "Join res: subscriber_primary=%d", join_res->subscriber_primary);

            esp_timer_start_periodic(sg->ping_timer, sg->ping_interval_ms * 1000);
            sg->options.on_join(join_res, sg->options.ctx);
            break;
        case LIVEKIT_PB_SIGNAL_RESPONSE_OFFER_TAG:
            sg->options.on_offer(res->message.offer.sdp, sg->options.ctx);
            break;
        case LIVEKIT_PB_SIGNAL_RESPONSE_ANSWER_TAG:
            sg->options.on_answer(res->message.answer.sdp, sg->options.ctx);
            break;
        case LIVEKIT_PB_SIGNAL_RESPONSE_TRICKLE_TAG:
            livekit_pb_trickle_request_t *trickle = &res->message.trickle;
            if (trickle->candidate_init == NULL) {
                ESP_LOGE(TAG, "Trickle candidate_init is NULL");
                break;
            }
            cJSON *candidate_init = NULL;
            do {
                candidate_init = cJSON_Parse(trickle->candidate_init);
                if (candidate_init == NULL) {
                    const char *error_ptr = cJSON_GetErrorPtr();
                    if (error_ptr != NULL) {
                        ESP_LOGE(TAG, "Failed to parse candidate_init: %s", error_ptr);
                    }
                    break;
                }
                cJSON *candidate = cJSON_GetObjectItemCaseSensitive(candidate_init, "candidate");
                if (!cJSON_IsString(candidate) || (candidate->valuestring == NULL)) {
                    ESP_LOGE(TAG, "Missing candidate key in candidate_init");
                    break;
                }
                ESP_LOGI(TAG, "Received trickle: target=%d, candidate=%s, final=%d",
                    trickle->target,
                    candidate->valuestring,
                    trickle->final
                );
                sg->options.on_trickle(candidate->valuestring, trickle->target, sg->options.ctx);
            } while (0);
            cJSON_Delete(candidate_init);
            break;
        default:
            break;
    }
    pb_release(LIVEKIT_PB_SIGNAL_RESPONSE_FIELDS, res);
}

static void on_data(livekit_sig_t *sg, const char *data, size_t len)
{
    ESP_LOGI(TAG, "Incoming res: %d byte(s)", len);
    livekit_pb_signal_response_t res = {};
    pb_istream_t stream = pb_istream_from_buffer((const pb_byte_t *)data, len);
    if (!pb_decode(&stream, LIVEKIT_PB_SIGNAL_RESPONSE_FIELDS, &res)) {
        ESP_LOGE(TAG, "Failed to decode res: %s", stream.errmsg);
        return;
    }

    ESP_LOGI(TAG, "Decoded res: type=%s(%d)",
        livekit_protocol_sig_res_name(res.which_message),
        res.which_message);
    handle_res(sg, &res);
}

static void on_ws_event(void *ctx, esp_event_base_t base, int32_t event_id, void *event_data)
{
    assert(ctx != NULL);
    livekit_sig_t *sg = (livekit_sig_t *)ctx;
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
                case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Signaling connected");
            sg->options.on_connect(sg->options.ctx);
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Signaling disconnected");
            esp_timer_stop(sg->ping_timer);

            log_error_if_nonzero("HTTP status code", data->error_handle.esp_ws_handshake_status_code);
            if (data->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) {
                log_error_if_nonzero("reported from esp-tls", data->error_handle.esp_tls_last_esp_err);
                log_error_if_nonzero("reported from tls stack", data->error_handle.esp_tls_stack_err);
                log_error_if_nonzero("captured as transport's socket errno", data->error_handle.esp_transport_sock_errno);
            }
            sg->options.on_disconnect(sg->options.ctx);
            break;
        case WEBSOCKET_EVENT_DATA:
            if (data->op_code != WS_TRANSPORT_OPCODES_BINARY) {
                ESP_LOGD(TAG, "Message: opcode=%d, len=%d", data->op_code, data->data_len);
                break;
            }
            if (data->data_len < 1) break;
            on_data(sg, data->data_ptr, data->data_len);
            break;
        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "Failed to connect to server");
            esp_timer_stop(sg->ping_timer);

            log_error_if_nonzero("HTTP status code", data->error_handle.esp_ws_handshake_status_code);
            if (data->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) {
                log_error_if_nonzero("reported from esp-tls", data->error_handle.esp_tls_last_esp_err);
                log_error_if_nonzero("reported from tls stack", data->error_handle.esp_tls_stack_err);
                log_error_if_nonzero("captured as transport's socket errno", data->error_handle.esp_transport_sock_errno);
            }
            sg->options.on_error(sg->options.ctx);
            break;
        default: break;
    }
}

livekit_sig_err_t livekit_sig_create(livekit_sig_handle_t *handle, livekit_sig_options_t *options)
{
    if (options == NULL || handle == NULL) {
        return LIVEKIT_SIG_ERR_INVALID_ARG;
    }

    if (options->on_connect    == NULL ||
        options->on_disconnect == NULL ||
        options->on_error      == NULL ||
        options->on_join       == NULL ||
        options->on_offer      == NULL ||
        options->on_answer     == NULL ||
        options->on_trickle    == NULL
    ) {
        ESP_LOGE(TAG, "Missing required event handlers");
        return LIVEKIT_SIG_ERR_INVALID_ARG;
    }

    livekit_sig_t *sg = calloc(1, sizeof(livekit_sig_t));
    if (sg == NULL) {
        return LIVEKIT_SIG_ERR_NO_MEM;
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
        return LIVEKIT_SIG_ERR_OTHER;
    }

    // URL will be set on connect
    sg->ws = esp_websocket_client_init(&default_ws_cfg);
    if (sg->ws == NULL) {
        ESP_LOGE(TAG, "Failed to initialize WebSocket client");
        esp_timer_delete(sg->ping_timer);
        free(sg);
        return LIVEKIT_SIG_ERR_WEBSOCKET;
    }
    esp_websocket_register_events(
        sg->ws,
        WEBSOCKET_EVENT_ANY,
        on_ws_event,
        (void *)sg
    );
    *handle = sg;
    return LIVEKIT_SIG_ERR_NONE;
}

livekit_sig_err_t livekit_sig_destroy(livekit_sig_handle_t handle)
{
    if (handle == NULL) {
        return LIVEKIT_SIG_ERR_INVALID_ARG;
    }
    livekit_sig_t *sg = (livekit_sig_t *)handle;
    esp_timer_delete(sg->ping_timer);
    livekit_sig_close(handle);
    esp_websocket_client_destroy(sg->ws);
    free(sg);
    return LIVEKIT_SIG_ERR_NONE;
}

livekit_sig_err_t livekit_sig_connect(livekit_sig_handle_t handle, const char* server_url, const char* token)
{
    if (server_url == NULL || token == NULL || handle == NULL) {
        return LIVEKIT_SIG_ERR_INVALID_ARG;
    }
    livekit_sig_t *sg = (livekit_sig_t *)handle;

    if (!livekit_url_build(server_url, token, &sg->url)) {
        return LIVEKIT_SIG_ERR_INVALID_URL;
    }

    if (esp_websocket_client_set_uri(sg->ws, sg->url) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WebSocket URI");
        return LIVEKIT_SIG_ERR_WEBSOCKET;
    }
    if (esp_websocket_client_start(sg->ws) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WebSocket");
        return LIVEKIT_SIG_ERR_WEBSOCKET;
    }
    return LIVEKIT_SIG_ERR_NONE;
}

livekit_sig_err_t livekit_sig_close(livekit_sig_handle_t handle)
{
    if (handle == NULL) {
        return LIVEKIT_SIG_ERR_INVALID_ARG;
    }
    livekit_sig_t *sg = (livekit_sig_t *)handle;

    esp_timer_stop(sg->ping_timer);
    if (esp_websocket_client_is_connected(sg->ws) &&
        esp_websocket_client_close(sg->ws, pdMS_TO_TICKS(LIVEKIT_SIG_WS_CLOSE_TIMEOUT_MS)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to close WebSocket");
        return LIVEKIT_SIG_ERR_WEBSOCKET;
    }
    if (sg->url != NULL) {
        free(sg->url);
        sg->url = NULL;
    }
    return LIVEKIT_SIG_ERR_NONE;
}

livekit_sig_err_t livekit_sig_send_leave(livekit_sig_handle_t handle)
{
    if (handle == NULL) {
        return LIVEKIT_SIG_ERR_INVALID_ARG;
    }
    livekit_sig_t *sg = (livekit_sig_t *)handle;
    livekit_pb_signal_request_t req = LIVEKIT_PB_SIGNAL_REQUEST_INIT_ZERO;
    req.which_message = LIVEKIT_PB_SIGNAL_REQUEST_LEAVE_TAG;

    livekit_pb_leave_request_t leave = {
        .reason = LIVEKIT_PB_DISCONNECT_REASON_CLIENT_INITIATED,
        .action = LIVEKIT_PB_LEAVE_REQUEST_ACTION_DISCONNECT,
        .can_reconnect = false
    };
    req.message.leave = leave;
    return send_request(sg, &req);
}

livekit_sig_err_t livekit_sig_send_answer(livekit_sig_handle_t handle, const char *sdp)
{
    if (sdp == NULL || handle == NULL) {
        return LIVEKIT_SIG_ERR_INVALID_ARG;
    }
    livekit_sig_t *sg = (livekit_sig_t *)handle;
    livekit_pb_signal_request_t req = LIVEKIT_PB_SIGNAL_REQUEST_INIT_ZERO;

    livekit_pb_session_description_t desc = {
        .type = "answer",
        .sdp = sdp
    };
    req.which_message = LIVEKIT_PB_SIGNAL_REQUEST_ANSWER_TAG;
    req.message.answer = desc;
    return send_request(sg, &req);
}

livekit_sig_err_t livekit_sig_send_offer(livekit_sig_handle_t handle, const char *sdp)
{
    if (sdp == NULL || handle == NULL) {
        return LIVEKIT_SIG_ERR_INVALID_ARG;
    }
    livekit_sig_t *sg = (livekit_sig_t *)handle;
    livekit_pb_signal_request_t req = LIVEKIT_PB_SIGNAL_REQUEST_INIT_ZERO;

    livekit_pb_session_description_t desc = {
        .type = "offer",
        .sdp = sdp
    };
    req.which_message = LIVEKIT_PB_SIGNAL_REQUEST_OFFER_TAG;
    req.message.offer = desc;
    return send_request(sg, &req);
}

livekit_sig_err_t livekit_sig_send_add_track(livekit_sig_handle_t handle, livekit_pb_add_track_request_t *add_track_req)
{
    if (handle == NULL || add_track_req == NULL) {
        return LIVEKIT_SIG_ERR_INVALID_ARG;
    }
    livekit_sig_t *sg = (livekit_sig_t *)handle;
    livekit_pb_signal_request_t req = LIVEKIT_PB_SIGNAL_REQUEST_INIT_ZERO;
    req.which_message = LIVEKIT_PB_SIGNAL_REQUEST_ADD_TRACK_TAG;
    req.message.add_track = *add_track_req;
    return send_request(sg, &req);
}