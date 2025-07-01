#include <stdlib.h>
#include <esp_log.h>
#include "esp_peer.h"
#include "engine.h"

#include "livekit.h"

static const char *TAG = "livekit";

typedef struct {
    livekit_eng_handle_t engine;
} livekit_room_t;

static void populate_media_options(
    livekit_eng_media_options_t *media_options,
    const livekit_pub_options_t *pub_options,
    const livekit_sub_options_t *sub_options)
{
    if (pub_options->kind & LIVEKIT_MEDIA_TYPE_AUDIO) {
        media_options->audio_dir |= ESP_PEER_MEDIA_DIR_SEND_ONLY;

        esp_peer_audio_codec_t codec = ESP_PEER_AUDIO_CODEC_NONE;
        switch (pub_options->audio_encode.codec) {
            case LIVEKIT_AUDIO_CODEC_G711A:
                codec = ESP_PEER_AUDIO_CODEC_G711A;
                break;
            case LIVEKIT_AUDIO_CODEC_G711U:
                codec = ESP_PEER_AUDIO_CODEC_G711U;
                break;
            case LIVEKIT_AUDIO_CODEC_OPUS:
                codec = ESP_PEER_AUDIO_CODEC_OPUS;
                break;
            default:
                ESP_LOGE(TAG, "Unsupported audio codec");
                break;
        }
        media_options->audio_info.codec = codec;
        media_options->audio_info.sample_rate = pub_options->audio_encode.sample_rate;
        media_options->audio_info.channel = pub_options->audio_encode.channel_count;
    }
    if (pub_options->kind & LIVEKIT_MEDIA_TYPE_VIDEO) {
        media_options->video_dir |= ESP_PEER_MEDIA_DIR_SEND_ONLY;
        esp_peer_video_codec_t codec = ESP_PEER_VIDEO_CODEC_NONE;
        switch (pub_options->video_encode.codec) {
            case LIVEKIT_VIDEO_CODEC_H264:
                codec = ESP_PEER_VIDEO_CODEC_H264;
                break;
            default:
                ESP_LOGE(TAG, "Unsupported video codec");
                break;
        }
        media_options->video_info.codec = codec;
        media_options->video_info.width = pub_options->video_encode.width;
        media_options->video_info.height = pub_options->video_encode.height;
        media_options->video_info.fps = pub_options->video_encode.fps;
    }
    if (sub_options->kind & LIVEKIT_MEDIA_TYPE_AUDIO) {
        media_options->audio_dir |= ESP_PEER_MEDIA_DIR_RECV_ONLY;
    }
    if (sub_options->kind & LIVEKIT_MEDIA_TYPE_VIDEO) {
        media_options->video_dir |= ESP_PEER_MEDIA_DIR_RECV_ONLY;
    }
    media_options->capturer = pub_options->capturer;
    media_options->renderer = sub_options->renderer;
}

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

static void on_eng_user_packet(livekit_pb_user_packet_t* packet, void *ctx)
{
    ESP_LOGI(TAG, "Received engine room update event");
    // TODO: Implement
}

static void on_eng_rpc_request(livekit_pb_rpc_request_t* req, void *ctx)
{
    ESP_LOGI(TAG, "Received engine data event");
    // TODO: Implement
}

static void on_eng_rpc_response(livekit_pb_rpc_response_t* res, void *ctx)
{
    ESP_LOGI(TAG, "Received engine RPC response event");
    // TODO: Implement
}

static void on_eng_rpc_ack(livekit_pb_rpc_ack_t* ack, void *ctx)
{
    ESP_LOGI(TAG, "Received engine RPC ack event");
    // TODO: Implement
}

static void on_eng_stream_header(livekit_pb_data_stream_header_t* header, void *ctx)
{
    ESP_LOGI(TAG, "Received engine stream header event");
    // TODO: Implement
}

static void on_eng_stream_chunk(livekit_pb_data_stream_chunk_t* chunk, void *ctx)
{
    ESP_LOGI(TAG, "Received engine stream chunk event");
    // TODO: Implement
}

static void on_eng_stream_trailer(livekit_pb_data_stream_trailer_t* trailer, void *ctx)
{
    ESP_LOGI(TAG, "Received engine stream trailer event");
    // TODO: Implement
}

livekit_err_t livekit_room_create(livekit_room_handle_t *handle, const livekit_room_options_t *options)
{
    if (handle == NULL || options == NULL) {
        return LIVEKIT_ERR_INVALID_ARG;
    }

    // Validate options
    if (options->publish.kind != LIVEKIT_MEDIA_TYPE_NONE &&
        options->publish.capturer == NULL) {
        ESP_LOGE(TAG, "Capturer must be set for media publishing");
        return LIVEKIT_ERR_INVALID_ARG;
    }
    if (options->subscribe.kind != LIVEKIT_MEDIA_TYPE_NONE &&
        options->subscribe.renderer == NULL) {
        ESP_LOGE(TAG, "Renderer must be set for subscribing to media");
        return LIVEKIT_ERR_INVALID_ARG;
    }
    if ((options->publish.kind & LIVEKIT_MEDIA_TYPE_AUDIO) &&
        (options->publish.audio_encode.codec == LIVEKIT_AUDIO_CODEC_NONE)) {
        ESP_LOGE(TAG, "Encode options must be set for audio publishing");
        return LIVEKIT_ERR_INVALID_ARG;
    }
    if ((options->publish.kind & LIVEKIT_MEDIA_TYPE_VIDEO) &&
        options->publish.video_encode.codec == LIVEKIT_VIDEO_CODEC_NONE) {
        ESP_LOGE(TAG, "Encode options must be set for video publishing");
        return LIVEKIT_ERR_INVALID_ARG;
    }

    livekit_room_t *room = calloc(1, sizeof(livekit_room_t));
    if (room == NULL) {
        return LIVEKIT_ERR_NO_MEM;
    }

    livekit_eng_media_options_t media_options = {};
    populate_media_options(&media_options, &options->publish, &options->subscribe);

    livekit_eng_options_t eng_options = {
        .media = media_options,
        .on_connected = on_eng_connected,
        .on_disconnected = on_eng_disconnected,
        .on_error = on_eng_error,
        .on_rpc_request = on_eng_rpc_request,
        .on_rpc_response = on_eng_rpc_response,
        .on_rpc_ack = on_eng_rpc_ack,
        .on_stream_header = on_eng_stream_header,
        .on_stream_chunk = on_eng_stream_chunk,
        .on_stream_trailer = on_eng_stream_trailer,
        .ctx = room
    };

    int ret = LIVEKIT_ERR_OTHER;
    do {
        if (livekit_eng_create(&room->engine, &eng_options) != LIVEKIT_ENG_ERR_NONE) {
            ESP_LOGE(TAG, "Failed to create engine");
            ret = LIVEKIT_ERR_ENGINE;
            break;
        }
        *handle = (livekit_room_handle_t)room;
        return LIVEKIT_ERR_NONE;
    } while (0);

    free(room);
    return ret;
}

livekit_err_t livekit_room_destroy(livekit_room_handle_t handle)
{
    livekit_room_t *room = (livekit_room_t *)handle;
    if (room == NULL) {
        return LIVEKIT_ERR_INVALID_ARG;
    }
    livekit_room_close(handle);
    livekit_eng_destroy(room->engine);
    free(room);
    return LIVEKIT_ERR_NONE;
}

livekit_err_t livekit_room_connect(livekit_room_handle_t handle, const char *server_url, const char *token)
{
    if (handle == NULL || server_url == NULL || token == NULL) {
        return LIVEKIT_ERR_INVALID_ARG;
    }
    livekit_room_t *room = (livekit_room_t *)handle;

    if (livekit_eng_connect(room->engine, server_url, token) != LIVEKIT_ENG_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to connect engine");
        return LIVEKIT_ERR_OTHER;
    }
    return LIVEKIT_ERR_NONE;
}

livekit_err_t livekit_room_close(livekit_room_handle_t handle)
{
    if (handle == NULL) {
        return LIVEKIT_ERR_INVALID_ARG;
    }
    livekit_room_t *room = (livekit_room_t *)handle;
    livekit_eng_close(room->engine);
    return LIVEKIT_ERR_NONE;
}

livekit_err_t livekit_room_publish_data(livekit_room_handle_t handle, livekit_payload_t *payload, livekit_publish_data_options_t *options)
{
    if (handle == NULL || payload == NULL || options == NULL) {
        return LIVEKIT_ERR_INVALID_ARG;
    }
    livekit_room_t *room = (livekit_room_t *)handle;

    // TODO: Can this be done without allocating additional memory?
    pb_bytes_array_t *bytes_array = malloc(PB_BYTES_ARRAY_T_ALLOCSIZE(payload->size));
    if (bytes_array == NULL) {
        return LIVEKIT_ERR_NO_MEM;
    }
    bytes_array->size = payload->size;
    memcpy(bytes_array->bytes, payload->bytes, payload->size);

    livekit_pb_user_packet_t user_packet = {
        .topic = options->topic,
        .payload = bytes_array
    };
    livekit_pb_data_packet_t packet = LIVEKIT_PB_DATA_PACKET_INIT_ZERO;
    packet.which_value = LIVEKIT_PB_DATA_PACKET_USER_TAG;
    packet.value.user = user_packet;

    packet.destination_identities_count = options->destination_identities_count;
    packet.destination_identities = options->destination_identities;
    // TODO: Set sender identity

    livekit_pb_data_packet_kind_t kind = options->lossy ?
        LIVEKIT_PB_DATA_PACKET_KIND_LOSSY : LIVEKIT_PB_DATA_PACKET_KIND_RELIABLE;

    if (livekit_eng_send_data_packet(room->engine, &packet, kind) != LIVEKIT_ENG_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to send data packet");
        free(bytes_array);
        return LIVEKIT_ERR_ENGINE;
    }
    free(bytes_array);
    return LIVEKIT_ERR_NONE;
}