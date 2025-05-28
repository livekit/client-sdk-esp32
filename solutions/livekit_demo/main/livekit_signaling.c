#include "livekit_signaling.h"

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(LK_TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void livekit_sig_handle_res(livekit_sig_t *sg, livekit_signal_response_t *res)
{
    bool should_forward = false;
    switch (res->which_message) {
        case LIVEKIT_SIGNAL_RESPONSE_PONG_TAG:
        case LIVEKIT_SIGNAL_RESPONSE_PONG_RESP_TAG:
            // TODO: Reset ping timeout
            break;
        case LIVEKIT_SIGNAL_RESPONSE_REFRESH_TOKEN_TAG:
            // TODO: Handle refresh token
            break;

        case LIVEKIT_SIGNAL_RESPONSE_JOIN_TAG:
            livekit_join_response_t *join_res = &res->message.join;
            sg->ping_interval = join_res->ping_interval;
            sg->ping_timeout = join_res->ping_timeout;
            // TODO: Start ping interval
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
            ESP_LOGI(LK_TAG, "Unknown signal res type");
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
    ESP_LOGI(LK_TAG, "Incoming signal res: %d byte(s)", len);
    if (len > LIVEKIT_SIG_RES_MAX_SIZE) {
        ESP_LOGE(LK_TAG,
            "Signal res too large: received %d, max %d",
            len,
            LIVEKIT_SIG_RES_MAX_SIZE
        );
        return;
    }
    livekit_signal_response_t res = {};
    pb_istream_t stream = pb_istream_from_buffer((const pb_byte_t *)data, len);
    if (!pb_decode(&stream, LIVEKIT_SIGNAL_RESPONSE_FIELDS, &res)) {
        ESP_LOGE(LK_TAG, "Failed to decode signal res: %s", stream.errmsg);
        return;
    }

    ESP_LOGI(LK_TAG, "Decoded signal res: type=%d", res.which_message);
    livekit_sig_handle_res(sg, &res);
    // TODO: Cleanup
}

void livekit_sig_event_handler(void *ctx, esp_event_base_t base, int32_t event_id, void *event_data)
{
    assert(ctx != NULL);
    livekit_sig_t *sg = (livekit_sig_t *)ctx;
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(LK_TAG, "Signaling connected");
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(LK_TAG, "Signaling disconnected");
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
                ESP_LOGD(LK_TAG, "Message, opcode=%d, len=%d", data->op_code, data->data_len);
                break;
            }
            if (data->data_len < 1) break;
            livekit_sig_on_data(sg, data->data_ptr, data->data_len);
            break;
        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(LK_TAG, "Failed to connect to server");
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

    ESP_LOGI(LK_TAG, "LiveKit signaling client created");
    // TODO:
    return 0;
}

static int livekit_sig_send_msg(esp_peer_signaling_handle_t h, esp_peer_signaling_msg_t *msg)
{
    ESP_LOGI(LK_TAG, "livekit_sig_send_msg");
    // TODO: Implement
    return 0;
}

const esp_peer_signaling_impl_t *livekit_sig_get_impl(void)
{
    ESP_LOGI(LK_TAG, "livekit_sig_get_impl");
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
        ESP_LOGE(LK_TAG, "URL cannot be empty");
        return -1;
    }

    if (strncmp(base_url, "ws://", 5) != 0 && strncmp(base_url, "wss://", 6) != 0) {
        ESP_LOGE(LK_TAG, "Unsupported URL scheme");
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
        ESP_LOGE(LK_TAG, "Final URL exceeds max length of %d", LIVEKIT_URL_MAX_LEN);
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
    ESP_LOGI(LK_TAG, "Signaling URL: %.*s[REDACTED]", (int)(final_len - token_len), *out_url);
    return 0;
}