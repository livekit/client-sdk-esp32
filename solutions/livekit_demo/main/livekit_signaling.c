#include "livekit_signaling.h"

void livekit_sig_on_connected(void *ctx)
{
    // TODO: Handle connected
    ESP_LOGI(LK_TAG, "livekit_sig_on_connected");
}

void livekit_sig_on_disconnected(void *ctx)
{
    // TODO: Handle disconnected
    ESP_LOGI(LK_TAG, "livekit_sig_on_disconnected");
}

void livekit_sig_on_data(void *ctx, const char *data, size_t len)
{
    // TODO: Handle data
    ESP_LOGI(LK_TAG, "livekit_sig_on_data");
}

void livekit_sig_event_handler(void *ctx, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            livekit_sig_on_connected(ctx);
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            livekit_sig_on_disconnected(ctx);
            break;
        case WEBSOCKET_EVENT_DATA:
            livekit_sig_on_data(ctx, data->data_ptr, data->data_len);
            break;
        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(LK_TAG, "WebSocket error");
            // TODO: Handle error
            break;
        default: break;
    }
}

static int livekit_sig_start(esp_peer_signaling_cfg_t *cfg, esp_peer_signaling_handle_t *h)
{
    if (cfg == NULL || cfg->signal_url == NULL || h == NULL) {
        return -1;
    }
    ESP_LOGI(LK_TAG, "livekit_sig_start");
    // TODO: Create signaling client
    return 0;
}

static int livekit_sig_send_msg(esp_peer_signaling_handle_t h, esp_peer_signaling_msg_t *msg)
{
    ESP_LOGI(LK_TAG, "livekit_sig_send_msg");
    // TODO: Implement
    return 0;
}

static int livekit_sig_stop(esp_peer_signaling_handle_t h)
{
    ESP_LOGI(LK_TAG, "livekit_sig_stop");
    // TODO: Free signaling client
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
    static const char url_format[] = "%s%srtc?sdk=%s&version=%s&access_token=%s&auto_subscribe=true";

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
    return 0;
}