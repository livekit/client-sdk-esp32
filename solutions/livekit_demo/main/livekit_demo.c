#include <stdlib.h>
#include <esp_log.h>
#include "livekit_demo.h"
#include "media_setup.h"

static const char *TAG = "livekit_demo";
static livekit_room_handle_t room_handle;

int join_room()
{
    livekit_room_options_t room_options = {
        .publish = {
            .kind = LIVEKIT_MEDIA_TYPE_AUDIO,
            .audio_encode = {
                .codec = LIVEKIT_AUDIO_CODEC_OPUS,
                .sample_rate = 16000,
                .channel_count = 1
            },
            .capturer = media_setup_get_capturer()
        }
    };

    if (room_handle != NULL) {
        ESP_LOGE(TAG, "Room already created");
        return -1;
    }
    if (livekit_room_create(&room_handle, &room_options) != LIVEKIT_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to create room");
        return -1;
    }

    // In a real application, you would fetch a token from your own server
    // and pass it to the room_connect function. To create a sandbox to easily run
    // this demo, see this guide: https://docs.livekit.io/home/cloud/sandbox/
    livekit_sandbox_res_t details;

#ifdef LK_TOKEN
    ESP_LOGI(TAG, "Using pre-defined LK_TOKEN");
    details.token = strdup(LK_TOKEN);
    details.server_url = strdup(LK_SERVER_URL);
    details.room_name = strdup(ROOM_NAME);
    details.participant_name = strdup(PARTICIPANT_NAME);
    ESP_LOGI(TAG, "LK_TOKEN: %s", details.token);
    ESP_LOGI(TAG, "LK_SERVER_URL: %s", details.server_url);
#else
    if (!livekit_sandbox_generate(LK_SANDBOX_ID, ROOM_NAME, PARTICIPANT_NAME, &details)) {
        ESP_LOGE(TAG, "Failed to generate sandbox token");
        return -1;
    }
#endif

    int connect_res = livekit_room_connect(room_handle, details.server_url, details.token);
    livekit_sandbox_res_free(&details);

    if (connect_res != LIVEKIT_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to connect to room");
        return -1;
    }
    return 0;
}

int leave_room()
{
    if (room_handle == NULL) {
        ESP_LOGE(TAG, "Room not created");
        return -1;
    }
    if (livekit_room_close(room_handle) != LIVEKIT_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to leave room");
        return -1;
    }
    if (livekit_room_destroy(room_handle) != LIVEKIT_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to destroy room");
        return -1;
    }
    room_handle = NULL;
    return 0;
}