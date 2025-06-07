#include <stdlib.h>
#include <esp_log.h>
#include "livekit_demo.h"

static const char *TAG = "livekit_demo";

static livekit_room_handle_t room_handle;
static livekit_room_options_t room_options = {};

int join_room()
{
    if (room_handle != NULL) {
        ESP_LOGE(TAG, "Room already created");
        return -1;
    }
    if (livekit_room_create(&room_handle, &room_options) != LIVEKIT_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to create room");
        return -1;
    }
    if (livekit_room_connect(room_handle, LK_SERVER_URL, LK_TOKEN) != LIVEKIT_ERR_NONE) {
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
    if (livekit_room_destroy(&room_handle) != LIVEKIT_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to destroy room");
        return -1;
    }
    room_handle = NULL;
    return 0;
}