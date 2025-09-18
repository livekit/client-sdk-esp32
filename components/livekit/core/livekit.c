/*
 * Copyright 2025 LiveKit, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <esp_log.h>
#include "esp_peer.h"
#include "engine.h"
#include "rpc_manager.h"
#include "system.h"
#include "livekit.h"

static const char *TAG = "livekit";

typedef struct {
    rpc_manager_handle_t rpc_manager;
    engine_handle_t engine;
    livekit_room_options_t options;
    livekit_connection_state_t state;
} livekit_room_t;

static bool send_reliable_packet(const livekit_pb_data_packet_t* packet, void *ctx)
{
    livekit_room_t *room = (livekit_room_t *)ctx;
    return engine_send_data_packet(room->engine, packet, true) == ENGINE_ERR_NONE;
}

static void on_rpc_result(const livekit_rpc_result_t* result, void* ctx)
{
    livekit_room_t *room = (livekit_room_t *)ctx;
    if (room->options.on_rpc_result != NULL) {
        room->options.on_rpc_result(result, room->options.ctx);
    }
}

static void on_user_packet(const livekit_pb_user_packet_t* packet, const char* sender_identity, void* ctx)
{
    livekit_room_t *room = (livekit_room_t *)ctx;
    if (room->options.on_data_received == NULL) {
        return;
    }
    livekit_data_received_t data = {
        .topic = packet->topic,
        .payload = {
            .bytes = packet->payload->bytes,
            .size = packet->payload->size
        },
        .sender_identity = (char*)sender_identity
    };
    room->options.on_data_received(&data, room->options.ctx);
}

static void populate_media_options(
    engine_media_options_t *media_options,
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

static void on_eng_state_changed(livekit_connection_state_t state, void *ctx)
{
    livekit_room_t *room = (livekit_room_t *)ctx;
    room->state = state;
    if (room->options.on_state_changed != NULL) {
        room->options.on_state_changed(state, room->options.ctx);
    }
}

static void on_eng_data_packet(livekit_pb_data_packet_t* packet, void *ctx)
{
    livekit_room_t *room = (livekit_room_t *)ctx;
    switch (packet->which_value) {
        case LIVEKIT_PB_DATA_PACKET_USER_TAG:
            on_user_packet(&packet->value.user, packet->participant_identity, ctx);
            break;
        case LIVEKIT_PB_DATA_PACKET_RPC_REQUEST_TAG:
        case LIVEKIT_PB_DATA_PACKET_RPC_ACK_TAG:
        case LIVEKIT_PB_DATA_PACKET_RPC_RESPONSE_TAG:
            rpc_manager_handle_packet(room->rpc_manager, packet);
            break;
        default:
            break;
    }
}

static void on_eng_room_info(const livekit_pb_room_t* info, void *ctx)
{
    livekit_room_t *room = (livekit_room_t *)ctx;
    if (room->options.on_room_info == NULL) {
        return;
    }
    const livekit_room_info_t room_info = {
        .sid = info->sid,
        .name = info->name,
        .metadata = info->metadata,
        .participant_count = info->num_participants,
        .active_recording = info->active_recording
    };
    room->options.on_room_info(&room_info, room->options.ctx);
}

static void on_eng_participant_info(const livekit_pb_participant_info_t* info, bool is_local, void *ctx)
{
    livekit_room_t *room = (livekit_room_t *)ctx;
    if (room->options.on_participant_info == NULL) {
        return;
    }
    const livekit_participant_info_t participant_info = {
        .sid = info->sid,
        .identity = info->identity,
        .name = info->name,
        .metadata = info->metadata,
        // Assumes enum values are the same as defined in the protocol.
        .kind = (livekit_participant_kind_t)info->kind,
        .state = (livekit_participant_state_t)info->state,
    };
    room->options.on_participant_info(&participant_info, room->options.ctx);
}

livekit_err_t livekit_room_create(livekit_room_handle_t *handle, const livekit_room_options_t *options)
{
    if (handle == NULL || options == NULL) {
        return LIVEKIT_ERR_INVALID_ARG;
    }
    if (!system_init_is_done()) {
        ESP_LOGE(TAG, "System initialization not performed or failed");
        return LIVEKIT_ERR_SYSTEM_INIT;
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
    room->state = LIVEKIT_CONNECTION_STATE_DISCONNECTED;
    room->options = *options;

    engine_media_options_t media_options = {};
    populate_media_options(&media_options, &options->publish, &options->subscribe);

    engine_options_t eng_options = {
        .media = media_options,
        .on_state_changed = on_eng_state_changed,
        .on_data_packet = on_eng_data_packet,
        .on_room_info = on_eng_room_info,
        .on_participant_info = on_eng_participant_info,
        .ctx = room
    };

    int ret = LIVEKIT_ERR_OTHER;
    do {
        room->engine = engine_init(&eng_options);
        if (room->engine == NULL) {
            ESP_LOGE(TAG, "Failed to create engine");
            ret = LIVEKIT_ERR_ENGINE;
            break;
        }
        rpc_manager_options_t rpc_manager_options = {
            .on_result = on_rpc_result,
            .send_packet = send_reliable_packet,
            .ctx = room
        };
        if (rpc_manager_create(&room->rpc_manager, &rpc_manager_options) != RPC_MANAGER_ERR_NONE) {
            ESP_LOGE(TAG, "Failed to create RPC manager");
            ret = LIVEKIT_ERR_OTHER;
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
    engine_destroy(room->engine);
    free(room);
    return LIVEKIT_ERR_NONE;
}

livekit_err_t livekit_room_connect(livekit_room_handle_t handle, const char *server_url, const char *token)
{
    if (handle == NULL || server_url == NULL || token == NULL) {
        return LIVEKIT_ERR_INVALID_ARG;
    }
    livekit_room_t *room = (livekit_room_t *)handle;

    if (engine_connect(room->engine, server_url, token) != ENGINE_ERR_NONE) {
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
    engine_close(room->engine);
    return LIVEKIT_ERR_NONE;
}

livekit_connection_state_t livekit_room_get_state(livekit_room_handle_t handle)
{
    if (handle == NULL) {
        return LIVEKIT_CONNECTION_STATE_DISCONNECTED;
    }
    livekit_room_t *room = (livekit_room_t *)handle;
    return room->state;
}

const char* livekit_connection_state_str(livekit_connection_state_t state)
{
    switch (state) {
        case LIVEKIT_CONNECTION_STATE_DISCONNECTED: return "Disconnected";
        case LIVEKIT_CONNECTION_STATE_CONNECTING:   return "Connecting";
        case LIVEKIT_CONNECTION_STATE_CONNECTED:    return "Connected";
        case LIVEKIT_CONNECTION_STATE_RECONNECTING: return "Reconnecting";
        case LIVEKIT_CONNECTION_STATE_FAILED:       return "Failed";
        default:                                    return "Unknown";
    }
}

const char* livekit_failure_reason_str(livekit_failure_reason_t reason)
{
    switch (reason) {
        case LIVEKIT_FAILURE_REASON_NONE:                 return "None";
        case LIVEKIT_FAILURE_REASON_UNREACHABLE:          return "Unreachable";
        case LIVEKIT_FAILURE_REASON_BAD_TOKEN:            return "Bad Token";
        case LIVEKIT_FAILURE_REASON_UNAUTHORIZED:         return "Unauthorized";
        case LIVEKIT_FAILURE_REASON_RTC:                  return "RTC";
        case LIVEKIT_FAILURE_REASON_MAX_RETRIES:          return "Max Retries";
        case LIVEKIT_FAILURE_REASON_PING_TIMEOUT:         return "Ping Timeout";
        case LIVEKIT_FAILURE_REASON_DUPLICATE_IDENTITY:   return "Duplicate Identity";
        case LIVEKIT_FAILURE_REASON_SERVER_SHUTDOWN:      return "Server Shutdown";
        case LIVEKIT_FAILURE_REASON_PARTICIPANT_REMOVED:  return "Participant Removed";
        case LIVEKIT_FAILURE_REASON_ROOM_DELETED:         return "Room Deleted";
        case LIVEKIT_FAILURE_REASON_STATE_MISMATCH:       return "State Mismatch";
        case LIVEKIT_FAILURE_REASON_JOIN_INCOMPLETE:      return "Join Incomplete";
        case LIVEKIT_FAILURE_REASON_MIGRATION:            return "Migration";
        case LIVEKIT_FAILURE_REASON_SIGNAL_CLOSE:         return "Signal Close";
        case LIVEKIT_FAILURE_REASON_ROOM_CLOSED:          return "Room Closed";
        case LIVEKIT_FAILURE_REASON_SIP_USER_UNAVAILABLE: return "SIP User Unavailable";
        case LIVEKIT_FAILURE_REASON_SIP_USER_REJECTED:    return "SIP User Rejected";
        case LIVEKIT_FAILURE_REASON_SIP_TRUNK_FAILURE:    return "SIP Trunk Failure";
        case LIVEKIT_FAILURE_REASON_CONNECTION_TIMEOUT:   return "Connection Timeout";
        case LIVEKIT_FAILURE_REASON_MEDIA_FAILURE:        return "Media Failure";
        default:                                          return "Other";
    }
}

livekit_failure_reason_t livekit_room_get_failure_reason(livekit_room_handle_t handle)
{
    if (handle == NULL) {
        return LIVEKIT_FAILURE_REASON_NONE;
    }
    livekit_room_t *room = (livekit_room_t *)handle;
    return engine_get_failure_reason(room->engine);
}

livekit_err_t livekit_room_publish_data(livekit_room_handle_t handle, livekit_data_publish_options_t *options)
{
    if (handle == NULL || options == NULL || options->payload == NULL) {
        return LIVEKIT_ERR_INVALID_ARG;
    }
    livekit_room_t *room = (livekit_room_t *)handle;

    // TODO: Can this be done without allocating additional memory?
    pb_bytes_array_t *bytes_array = malloc(PB_BYTES_ARRAY_T_ALLOCSIZE(options->payload->size));
    if (bytes_array == NULL) {
        return LIVEKIT_ERR_NO_MEM;
    }
    bytes_array->size = (pb_size_t)options->payload->size;
    memcpy(bytes_array->bytes, options->payload->bytes, options->payload->size);

    livekit_pb_user_packet_t user_packet = {
        .topic = options->topic,
        .payload = bytes_array
    };
    livekit_pb_data_packet_t packet = LIVEKIT_PB_DATA_PACKET_INIT_ZERO;
    packet.which_value = LIVEKIT_PB_DATA_PACKET_USER_TAG;
    packet.value.user = user_packet;

    packet.destination_identities_count = (pb_size_t)options->destination_identities_count;
    packet.destination_identities = options->destination_identities;
    // TODO: Set sender identity

    if (engine_send_data_packet(room->engine, &packet, !options->lossy) != ENGINE_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to send data packet");
        free(bytes_array);
        return LIVEKIT_ERR_ENGINE;
    }
    free(bytes_array);
    return LIVEKIT_ERR_NONE;
}

livekit_err_t livekit_room_rpc_register(livekit_room_handle_t handle, const char* method, livekit_rpc_handler_t handler)
{
    if (handle == NULL || method == NULL || handler == NULL) {
        return LIVEKIT_ERR_INVALID_ARG;
    }
    livekit_room_t *room = (livekit_room_t *)handle;

    if (rpc_manager_register(room->rpc_manager, method, handler) != RPC_MANAGER_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to register RPC method '%s'", method);
        return LIVEKIT_ERR_INVALID_STATE;
    }
    return LIVEKIT_ERR_NONE;
}

livekit_err_t livekit_room_rpc_unregister(livekit_room_handle_t handle, const char* method)
{
    if (handle == NULL || method == NULL) {
        return LIVEKIT_ERR_INVALID_ARG;
    }
    livekit_room_t *room = (livekit_room_t *)handle;

    if (rpc_manager_unregister(room->rpc_manager, method) != RPC_MANAGER_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to unregister RPC method '%s'", method);
        return LIVEKIT_ERR_INVALID_STATE;
    }
    return LIVEKIT_ERR_NONE;
}

livekit_err_t livekit_system_init(void)
{
    esp_err_t ret = system_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "System initialization failed");
        return ret;
    }
    return LIVEKIT_ERR_NONE;
}