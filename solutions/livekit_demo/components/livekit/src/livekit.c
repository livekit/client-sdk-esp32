#include <stdlib.h>
#include <esp_log.h>
#include <livekit_engine.h>
#include "livekit.h"

static const char *TAG = "livekit";

typedef struct {
    // TODO: Add fields here
} livekit_room_t;

livekit_err_t livekit_room_create(livekit_room_handle_t *handle)
{
    livekit_room_t *room = (livekit_room_t *)malloc(sizeof(livekit_room_t));
    if (room == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for new room");
        return LIVEKIT_ERR_NO_MEM;
    }
    *handle = (livekit_room_handle_t)room;
    return LIVEKIT_ERR_NONE;
}

livekit_err_t livekit_room_destroy(livekit_room_handle_t handle)
{
    livekit_room_t *room = (livekit_room_t *)handle;
    if (room == NULL) {
        return LIVEKIT_ERR_INVALID_ARG;
    }
    // TODO: Leave room if not already, free other resources
    free(room);
    return LIVEKIT_ERR_NONE;
}

livekit_err_t livekit_room_join(livekit_room_handle_t handle, livekit_join_options_t *options)
{
    // TODO: Implement
    return LIVEKIT_ERR_NONE;
}

livekit_err_t livekit_room_leave(livekit_room_handle_t handle)
{
    // TODO: Implement
    return LIVEKIT_ERR_NONE;
}