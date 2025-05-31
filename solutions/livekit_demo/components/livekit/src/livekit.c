#include <stdlib.h>
#include <esp_log.h>
#include <livekit_engine.h>
#include "livekit.h"

static const char *TAG = "livekit";

typedef struct {
    livekit_eng_handle_t engine;
    // TODO: Add fields here
} livekit_room_t;

static void on_eng_connected(livekit_eng_event_connected_t detail, void *ctx)
{
    ESP_LOGI(TAG, "Received engine connected event");
    // TODO: Implement
}

static void on_eng_disconnected(livekit_eng_event_disconnected_t detail, void *ctx)
{
    ESP_LOGI(TAG, "Received engine disconnected event");
    // TODO: Implement
}

static void on_eng_error(livekit_eng_event_error_t detail, void *ctx)
{
    ESP_LOGE(TAG, "Received engine error event");
    // TODO: Implement
}

static void on_eng_room_update(livekit_eng_event_room_update_t detail, void *ctx)
{
    ESP_LOGI(TAG, "Received engine room update event");
    // TODO: Implement
}

static void on_eng_data(livekit_eng_event_data_t detail, void *ctx)
{
    ESP_LOGI(TAG, "Received engine data event");
    // TODO: Implement
}

static void on_eng_rpc_request(livekit_eng_event_rpc_request_t detail, void *ctx)
{
    ESP_LOGI(TAG, "Received engine RPC request event");
    // TODO: Implement
}

static void on_eng_rpc_response(livekit_eng_event_rpc_response_t detail, void *ctx)
{
    ESP_LOGI(TAG, "Received engine RPC response event");
    // TODO: Implement
}

static void on_eng_rpc_ack(livekit_eng_event_rpc_ack_t detail, void *ctx)
{
    ESP_LOGI(TAG, "Received engine RPC ack event");
    // TODO: Implement
}

static void on_eng_stream_header(livekit_eng_event_stream_header_t detail, void *ctx)
{
    ESP_LOGI(TAG, "Received engine stream header event");
    // TODO: Implement
}

static void on_eng_stream_chunk(livekit_eng_event_stream_chunk_t detail, void *ctx)
{
    ESP_LOGI(TAG, "Received engine stream chunk event");
    // TODO: Implement
}

static void on_eng_stream_trailer(livekit_eng_event_stream_trailer_t detail, void *ctx)
{
    ESP_LOGI(TAG, "Received engine stream trailer event");
    // TODO: Implement
}

livekit_err_t livekit_room_create(livekit_room_handle_t *handle, livekit_room_options_t *options)
{
    livekit_room_t *room = (livekit_room_t *)malloc(sizeof(livekit_room_t));
    if (room == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for new room");
        return LIVEKIT_ERR_NO_MEM;
    }

    livekit_eng_options_t eng_options = {
        .ctx = room,
        .on_connected = on_eng_connected,
        .on_disconnected = on_eng_disconnected,
        .on_error = on_eng_error,
        .on_room_update = on_eng_room_update,
        .on_data = on_eng_data,
        .on_rpc_request = on_eng_rpc_request,
        .on_rpc_response = on_eng_rpc_response,
        .on_rpc_ack = on_eng_rpc_ack,
        .on_stream_header = on_eng_stream_header,
        .on_stream_chunk = on_eng_stream_chunk,
        .on_stream_trailer = on_eng_stream_trailer,
    };
    livekit_eng_create(&eng_options, &room->engine);
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
    livekit_eng_close(LIVEKIT_DISCONNECT_REASON_UNKNOWN_REASON, room->engine);
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