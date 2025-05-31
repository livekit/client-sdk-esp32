#include "esp_log.h"
#include "livekit_engine.h"
#include "livekit_signaling.h"

static const char *TAG = "livekit_engine";

typedef struct {
    livekit_eng_options_t options;
    livekit_sig_handle_t  sig;
} livekit_eng_t;

static void on_sig_connect(void *ctx)
{
    ESP_LOGI(TAG, "Received signaling client connected event");
    // TODO: Implement
}

static void on_sig_disconnect(void *ctx)
{
    ESP_LOGI(TAG, "Received signaling client disconnected event");
    // TODO: Implement
}

static void on_sig_error(void *ctx)
{
    ESP_LOGI(TAG, "Received signaling client error event");
    // TODO: Implement
}

static void on_sig_message(livekit_signal_response_t *message, void *ctx)
{
    ESP_LOGI(TAG, "Received signaling client message event");
    // TODO: Implement
}

int livekit_eng_create(livekit_eng_options_t *options, livekit_eng_handle_t *handle)
{
    if (options == NULL || handle == NULL) {
        return LIVEKIT_ENG_ERR_INVALID_ARG;
    }
    livekit_eng_t *eng = (livekit_eng_t *)calloc(1, sizeof(livekit_eng_t));
    if (eng == NULL) {
        return LIVEKIT_ENG_ERR_NO_MEM;
    }
    eng->options = *options;

    livekit_sig_options_t sig_options = {
        .ctx = eng,
        .on_connect = on_sig_connect,
        .on_disconnect = on_sig_disconnect,
        .on_error = on_sig_error,
        .on_message = on_sig_message,
    };
    if (livekit_sig_create(&sig_options, &eng->sig) != LIVEKIT_SIG_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to create signaling client");
        free(eng);
        return LIVEKIT_ENG_ERR_OTHER;
    }
    *handle = eng;
    return LIVEKIT_ENG_ERR_NONE;
}

int livekit_eng_destroy(livekit_eng_handle_t handle)
{
    if (handle == NULL) {
        return LIVEKIT_ENG_ERR_INVALID_ARG;
    }
    livekit_eng_t *eng = (livekit_eng_t *)handle;
    livekit_eng_close(LIVEKIT_DISCONNECT_REASON_UNKNOWN_REASON, handle);
    free(eng);
    return LIVEKIT_ENG_ERR_NONE;
}

int livekit_eng_connect(const char* server_url, const char* token, livekit_eng_handle_t handle)
{
    if (server_url == NULL || token == NULL || handle == NULL) {
        return LIVEKIT_ENG_ERR_INVALID_ARG;
    }
    livekit_eng_t *eng = (livekit_eng_t *)handle;

    if (livekit_sig_connect(server_url, token, eng->sig) != LIVEKIT_SIG_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to connect signaling client");
        return LIVEKIT_ENG_ERR_SIGNALING;
    }
    return LIVEKIT_ENG_ERR_NONE;
}

int livekit_eng_close(livekit_disconnect_reason_t reason, livekit_eng_handle_t handle)
{
    if (handle == NULL) {
        return LIVEKIT_ENG_ERR_INVALID_ARG;
    }
    livekit_eng_t *eng = (livekit_eng_t *)handle;

    // TODO: Send leave request

    if (livekit_sig_close(true, eng->sig) != LIVEKIT_SIG_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to close signaling client");
        return LIVEKIT_ENG_ERR_SIGNALING;
    }
    return LIVEKIT_ENG_ERR_NONE;
}

int livekit_eng_publish_data(livekit_data_packet_t packet, livekit_data_packet_kind_t kind, livekit_eng_handle_t handle)
{
    // TODO: Implement
    return 0;
}

int livekit_eng_send_request(livekit_signal_request_t request, livekit_eng_handle_t handle)
{
    // TODO: Implement
    return 0;
}

int livekit_eng_set_media_provider(livekit_eng_media_provider_t* provider, livekit_eng_handle_t handle)
{
    // TODO: Implement
    return 0;
}