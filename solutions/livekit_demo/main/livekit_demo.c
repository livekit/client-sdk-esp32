#include <stdlib.h>
#include <esp_log.h>
#include "livekit_demo.h"

static const char *TAG = "livekit_demo";

static livekit_room_handle_t room_handle;
static livekit_join_options_t join_options = {
    .server_url = LK_SERVER_URL,
    .token = LK_TOKEN
};

int join_room()
{
    if (room_handle != NULL) {
        ESP_LOGE(TAG, "Room already created");
        return -1;
    }
    if (livekit_room_create(&room_handle) != LIVEKIT_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to create room");
        return -1;
    }
    if (livekit_room_join(room_handle, &join_options) != LIVEKIT_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to join room");
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
    if (livekit_room_leave(room_handle) != LIVEKIT_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to leave room");
        return -1;
    }
    if (livekit_room_destroy(&room_handle) != LIVEKIT_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to destroy room");
        return -1;
    }
    return 0;
}