#include <sys/time.h>
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

#include <livekit_protocol.h>
#include "livekit_signaling.h"

static const char *TAG = "livekit_signaling";

typedef struct {
    esp_websocket_client_handle_t ws;
} livekit_wss_client_t;

typedef struct {
    livekit_wss_client_t          *wss_client;
    esp_peer_signaling_cfg_t      cfg;

    bool                          pinging;
    bool                          ping_stop;
    int32_t                       ping_timeout;
    int32_t                       ping_interval;
    int64_t                       rtt;
} livekit_sig_t;

static int64_t get_unix_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000LL + (tv.tv_usec / 1000LL);
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static int livekit_sig_send_req(livekit_sig_t *sg, livekit_signal_request_t *req, uint8_t *enc_buf, size_t enc_buf_size)
{
    pb_ostream_t stream = pb_ostream_from_buffer(enc_buf, enc_buf_size);

    if (!pb_encode(&stream, &livekit_signal_request_t_msg, req)) {
        ESP_LOGE(TAG, "Failed to encode request: %s", PB_GET_ERROR(&stream));
        return -1;
    }
    // TODO: Set send timeout
    if (esp_websocket_client_send_bin(sg->wss_client->ws, (const char *)enc_buf, stream.bytes_written, 0) < 0) {
        ESP_LOGE(TAG, "Failed to send request");
        return -1;
    }
    return 0;
}

static void livekit_sig_send_ping(livekit_sig_t *sg)
{
    int64_t timestamp = get_unix_time_ms();
    int64_t rtt = sg->rtt;
    ESP_LOGI(TAG, "Sending ping: timestamp=%" PRId64 "ms, rtt=%" PRId64 "ms", timestamp, rtt);

    livekit_signal_request_t req = LIVEKIT_SIGNAL_REQUEST_INIT_DEFAULT;
    req.which_message = LIVEKIT_SIGNAL_REQUEST_PING_REQ_TAG;
    req.message.ping_req.timestamp = timestamp;
    req.message.ping_req.rtt = rtt;

    uint8_t enc_buf[512];
    if (livekit_sig_send_req(sg, &req, enc_buf, sizeof(enc_buf)) != 0) {
        ESP_LOGE(TAG, "Failed to send ping");
        return;
    }
}

static void livekit_sig_ping_task(void *arg)
{
    assert(arg != NULL);
    livekit_sig_t *sg = (livekit_sig_t *)arg;
    ESP_LOGI(TAG, "Ping task started");

    while (!sg->ping_stop) {
        media_lib_thread_sleep(sg->ping_interval * 1000);
        livekit_sig_send_ping(sg);
    }
    media_lib_thread_destroy(NULL);
}

static int livekit_sig_start_ping_task(livekit_sig_t *sg)
{
    if (sg->pinging) return -1;
    sg->ping_stop = false;
    sg->pinging = false;

    media_lib_thread_handle_t handle;

    // Use larger stack size to accommodate livekit_signal_request_t. This type is
    // especially large because it contains a union of all possible messages (even though
    // the ping_req message is small).
    media_lib_thread_create(
        &handle,
        "ping",
        livekit_sig_ping_task,
        sg,
        8 * 1024,
        10, // MEDIA_LIB_DEFAULT_THREAD_PRIORITY
        0 // MEDIA_LIB_DEFAULT_THREAD_CORE
    );
    if (!handle) return -1;
    sg->pinging = true;
    return 0;
}

static int livekit_sig_stop_ping_task(livekit_sig_t *sg)
{
    sg->ping_stop = true;
    while (sg->pinging) {
        media_lib_thread_sleep(50);
    }
    return 0;
}

static void livekit_sig_handle_res(livekit_sig_t *sg, livekit_signal_response_t *res)
{
    bool should_forward = false;
    switch (res->which_message) {
        case LIVEKIT_SIGNAL_RESPONSE_PONG_RESP_TAG:
            livekit_pong_t *pong = &res->message.pong_resp;
            sg->rtt = get_unix_time_ms() - pong->last_ping_timestamp;
            // TODO: Reset ping timeout
            break;
        case LIVEKIT_SIGNAL_RESPONSE_REFRESH_TOKEN_TAG:
            // TODO: Handle refresh token
            break;

        case LIVEKIT_SIGNAL_RESPONSE_JOIN_TAG:
            livekit_join_response_t *join_res = &res->message.join;
            sg->ping_interval = join_res->ping_interval;
            sg->ping_timeout = join_res->ping_timeout;
            ESP_LOGI(TAG,
                "Join res: ping_interval=%" PRId32 "ms, ping_timeout=%" PRId32 "ms",
                sg->ping_interval,
                sg->ping_timeout
            );
            livekit_sig_start_ping_task(sg);
            should_forward = true;
            break;

        case LIVEKIT_SIGNAL_RESPONSE_RECONNECT_TAG:
        case LIVEKIT_SIGNAL_RESPONSE_ANSWER_TAG:
        case LIVEKIT_SIGNAL_RESPONSE_OFFER_TAG:
        case LIVEKIT_SIGNAL_RESPONSE_TRICKLE_TAG:
        case LIVEKIT_SIGNAL_RESPONSE_TRACK_PUBLISHED_TAG:
        case LIVEKIT_SIGNAL_RESPONSE_LEAVE_TAG:
        case LIVEKIT_SIGNAL_RESPONSE_UPDATE_TAG:
        case LIVEKIT_SIGNAL_RESPONSE_MUTE_TAG:
        case LIVEKIT_SIGNAL_RESPONSE_SPEAKERS_CHANGED_TAG:
        case LIVEKIT_SIGNAL_RESPONSE_ROOM_UPDATE_TAG:
        case LIVEKIT_SIGNAL_RESPONSE_CONNECTION_QUALITY_TAG:
        case LIVEKIT_SIGNAL_RESPONSE_STREAM_STATE_UPDATE_TAG:
        case LIVEKIT_SIGNAL_RESPONSE_SUBSCRIBED_QUALITY_UPDATE_TAG:
        case LIVEKIT_SIGNAL_RESPONSE_SUBSCRIPTION_PERMISSION_UPDATE_TAG:
        case LIVEKIT_SIGNAL_RESPONSE_TRACK_UNPUBLISHED_TAG:
        case LIVEKIT_SIGNAL_RESPONSE_SUBSCRIPTION_RESPONSE_TAG:
        case LIVEKIT_SIGNAL_RESPONSE_REQUEST_RESPONSE_TAG:
        case LIVEKIT_SIGNAL_RESPONSE_TRACK_SUBSCRIBED_TAG:
        case LIVEKIT_SIGNAL_RESPONSE_ROOM_MOVED_TAG:
            should_forward = true;
            break;
        default:
            ESP_LOGI(TAG, "Unknown signal res type");
    }
    if (should_forward) {
        esp_peer_signaling_msg_t msg = {
            .type = ESP_PEER_SIGNALING_MSG_CUSTOMIZED,
            .data = (uint8_t *)res,
            .size = sizeof(res),
        };
        sg->cfg.on_msg(&msg, sg->cfg.ctx);
    } else {
        pb_release(LIVEKIT_SIGNAL_RESPONSE_FIELDS, res);
    }
}

static void livekit_sig_on_data(livekit_sig_t *sg, const char *data, size_t len)
{
    ESP_LOGI(TAG, "Incoming signal res: %d byte(s)", len);
    if (len > LIVEKIT_SIG_RES_MAX_SIZE) {
        ESP_LOGE(TAG,
            "Signal res too large: received %d, max %d",
            len,
            LIVEKIT_SIG_RES_MAX_SIZE
        );
        return;
    }
    livekit_signal_response_t res = {};
    pb_istream_t stream = pb_istream_from_buffer((const pb_byte_t *)data, len);
    if (!pb_decode(&stream, LIVEKIT_SIGNAL_RESPONSE_FIELDS, &res)) {
        ESP_LOGE(TAG, "Failed to decode signal res: %s", stream.errmsg);
        return;
    }

    ESP_LOGI(TAG, "Decoded signal res: type=%d", res.which_message);
    livekit_sig_handle_res(sg, &res);
}

void livekit_sig_event_handler(void *ctx, esp_event_base_t base, int32_t event_id, void *event_data)
{
    assert(ctx != NULL);
    livekit_sig_t *sg = (livekit_sig_t *)ctx;
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Signaling connected");
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Signaling disconnected");
            log_error_if_nonzero("HTTP status code", data->error_handle.esp_ws_handshake_status_code);
            if (data->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) {
                log_error_if_nonzero("reported from esp-tls", data->error_handle.esp_tls_last_esp_err);
                log_error_if_nonzero("reported from tls stack", data->error_handle.esp_tls_stack_err);
                log_error_if_nonzero("captured as transport's socket errno", data->error_handle.esp_transport_sock_errno);
            }
            sg->cfg.on_close(sg->cfg.ctx);
            break;
        case WEBSOCKET_EVENT_DATA:
            if (data->op_code != WS_TRANSPORT_OPCODES_BINARY) {
                ESP_LOGD(TAG, "Message: opcode=%d, len=%d", data->op_code, data->data_len);
                break;
            }
            if (data->data_len < 1) break;
            livekit_sig_on_data(sg, data->data_ptr, data->data_len);
            break;
        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "Failed to connect to server");
            log_error_if_nonzero("HTTP status code", data->error_handle.esp_ws_handshake_status_code);
            if (data->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) {
                log_error_if_nonzero("reported from esp-tls", data->error_handle.esp_tls_last_esp_err);
                log_error_if_nonzero("reported from tls stack", data->error_handle.esp_tls_stack_err);
                log_error_if_nonzero("captured as transport's socket errno", data->error_handle.esp_transport_sock_errno);
            }
            break;
        default: break;
    }
}

int livekit_wss_create(livekit_sig_t *sg)
{
    assert(sg != NULL);
    livekit_wss_client_t *wss = calloc(1, sizeof(livekit_wss_client_t));
    if (wss == NULL)  return -1;

    esp_websocket_client_config_t ws_cfg = {
        .uri = sg->cfg.signal_url,
        .buffer_size = LIVEKIT_SIG_BUFFER_SIZE,
        .disable_pingpong_discon = true,
        .reconnect_timeout_ms = LIVEKIT_SIG_RECONNECT_TIMEOUT_MS,
        .network_timeout_ms = LIVEKIT_SIG_NETWORK_TIMEOUT_MS,
#ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
        .crt_bundle_attach = esp_crt_bundle_attach
#endif
    };

    wss->ws = esp_websocket_client_init(&ws_cfg);
    if (wss->ws == NULL) return -1;

    esp_websocket_register_events(
        wss->ws,
        WEBSOCKET_EVENT_ANY,
        livekit_sig_event_handler,
        (void *)sg
    );
    int ret = esp_websocket_client_start(wss->ws);
    if (ret != ESP_OK) return ret;

    sg->wss_client = wss;
    return 0;
}

static void livekit_wss_destroy(livekit_wss_client_t *wss)
{
    if (wss->ws) {
        esp_websocket_client_stop(wss->ws);
        esp_websocket_client_destroy(wss->ws);
    }
    free(wss);
}

static int livekit_sig_stop(esp_peer_signaling_handle_t h)
{
    livekit_sig_t *sg = (livekit_sig_t *)h;
    livekit_sig_stop_ping_task(sg);
    if (sg->wss_client) {
        livekit_wss_destroy(sg->wss_client);
    }
    free(sg);
    return 0;
}

static int livekit_sig_start(esp_peer_signaling_cfg_t *cfg, esp_peer_signaling_handle_t *h)
{
    if (cfg == NULL || cfg->signal_url == NULL || h == NULL) {
        return -1;
    }
    livekit_sig_t *sg = calloc(1, sizeof(livekit_sig_t));
    if (sg == NULL) {
        return -1;
    }

    // Copy configuration
    sg->cfg = *cfg;
    *h = sg;
    int ret = livekit_wss_create(sg);
    if (ret != ESP_OK) {
        *h = NULL;
        livekit_sig_stop(sg);
        return ret;
    }

    ESP_LOGI(TAG, "LiveKit signaling client created");
    return 0;
}

static int livekit_sig_send_msg(esp_peer_signaling_handle_t h, esp_peer_signaling_msg_t *msg)
{
    ESP_LOGI(TAG, "livekit_sig_send_msg");
    // TODO: Implement
    return 0;
}

const esp_peer_signaling_impl_t *livekit_sig_get_impl(void)
{
    ESP_LOGI(TAG, "livekit_sig_get_impl");
    static const esp_peer_signaling_impl_t impl = {
        .start = livekit_sig_start,
        .send_msg = livekit_sig_send_msg,
        .stop = livekit_sig_stop,
    };
    return &impl;
}

int livekit_sig_build_url(const char *base_url, const char *token, char **out_url)
{
    static const char url_format[] = "%s%srtc?sdk=%s&version=%s&auto_subscribe=true&access_token=%s";
    // Access token parameter must stay at the end for logging

    if (base_url == NULL || token == NULL || out_url == NULL) {
        return -1;
    }
    size_t url_len = strlen(base_url);
    if (url_len < 1) {
        ESP_LOGE(TAG, "URL cannot be empty");
        return -1;
    }

    if (strncmp(base_url, "ws://", 5) != 0 && strncmp(base_url, "wss://", 6) != 0) {
        ESP_LOGE(TAG, "Unsupported URL scheme");
        return -1;
    }
    // Do not add a trailing slash if the URL already has one
    const char *separator = base_url[url_len - 1] == '/' ? "" : "/";

    int final_len = snprintf(NULL, 0, url_format,
        base_url,
        separator,
        LIVEKIT_SDK_ID,
        LIVEKIT_SDK_VERSION,
        token
    );

    if (final_len >= LIVEKIT_URL_MAX_LEN) {
        ESP_LOGE(TAG, "Final URL exceeds max length of %d", LIVEKIT_URL_MAX_LEN);
        return -1;
    }

    *out_url = (char *)malloc(final_len + 1);
    if (*out_url == NULL) {
        return -1;
    }

    sprintf(*out_url, url_format,
        base_url,
        separator,
        LIVEKIT_SDK_ID,
        LIVEKIT_SDK_VERSION,
        token
    );

    // Token is redacted from logging for security
    size_t token_len = strlen(token);
    ESP_LOGI(TAG, "Signaling URL: %.*s[REDACTED]", (int)(final_len - token_len), *out_url);
    return 0;
}