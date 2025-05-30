#include <stdlib.h>
#include <esp_log.h>
#include <livekit_engine.h>
#include "livekit.h"

static const char *TAG = "livekit";

typedef struct {
    livekit_eng_handle_t engine;
    // TODO: Add fields here
} livekit_room_t;

livekit_err_t livekit_room_create(livekit_room_handle_t *handle, livekit_room_options_t *options)
{
    livekit_room_t *room = (livekit_room_t *)malloc(sizeof(livekit_room_t));
    if (room == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for new room");
        return LIVEKIT_ERR_NO_MEM;
    }

    livekit_eng_cfg_t cfg = {
        .peer_cfg = {
            .ctx = room
        },
        .signaling_cfg = {
            .signal_url = options->server_url,
            .ctx = room
        }
    };
    livekit_eng_open(&cfg, &room->engine);
    if (room->engine == NULL) {
        ESP_LOGE(TAG, "Failed to open engine");
        free(room);
        return LIVEKIT_ERR_OTHER;
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
    livekit_eng_close(room->engine);
    free(room);
    return LIVEKIT_ERR_NONE;
}

livekit_err_t livekit_room_connect(livekit_room_handle_t handle)
{
    // TODO: Implement
    return LIVEKIT_ERR_NONE;
}

livekit_err_t livekit_room_close(livekit_room_handle_t handle)
{
    // TODO: Implement
    return LIVEKIT_ERR_NONE;
}