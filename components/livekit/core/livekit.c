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
#include <string.h>
#include <esp_log.h>
#include <khash.h>
#include "esp_peer.h"
#include "engine.h"
#include "rpc_server_manager.h"
#include "data_stream_reader.h"
#include "data_stream_writer.h"
#include "system.h"
#include "livekit.h"

static const char *TAG = "livekit";

// Map from participant identity (heap-owned key) -> client_protocol value.
KHASH_MAP_INIT_STR(participant_protocols, int)

typedef struct {
    rpc_server_manager_handle_t rpc_server;
    data_stream_reader_handle_t data_stream_reader;
    data_stream_writer_handle_t data_stream_writer;
    engine_handle_t engine;
    livekit_room_options_t options;
    livekit_connection_state_t state;

    // Tracks each known participant's advertised client_protocol so we can
    // pick the right RPC transport per-peer. Keys are strdup'd on insert.
    khash_t(participant_protocols) *participant_protocols;

    // Strdup'd identity of the local participant, set on join and cleared
    // on disconnect.
    char *local_identity;
} livekit_room_t;

/// Insert or update the registry entry for `identity`.
static void participant_registry_set(livekit_room_t *room, const char *identity, int client_protocol)
{
    if (room->participant_protocols == NULL || identity == NULL) {
        return;
    }
    khiter_t k = kh_get(participant_protocols, room->participant_protocols, identity);
    if (k == kh_end(room->participant_protocols)) {
        char *owned = strdup(identity);
        if (owned == NULL) {
            return;
        }
        int put_flag;
        k = kh_put(participant_protocols, room->participant_protocols, owned, &put_flag);
        if (put_flag < 0) {
            free(owned);
            return;
        }
    }
    kh_value(room->participant_protocols, k) = client_protocol;
}

/// Drop the registry entry for `identity`, freeing the owned key.
static void participant_registry_remove(livekit_room_t *room, const char *identity)
{
    if (room->participant_protocols == NULL || identity == NULL) {
        return;
    }
    khiter_t k = kh_get(participant_protocols, room->participant_protocols, identity);
    if (k == kh_end(room->participant_protocols)) {
        return;
    }
    const char *key = kh_key(room->participant_protocols, k);
    kh_del(participant_protocols, room->participant_protocols, k);
    free((void *)key);
}

/// Look up `identity`'s client_protocol, falling back to DEFAULT for unknown
/// or unregistered participants.
static int participant_registry_get(livekit_room_t *room, const char *identity)
{
    if (room->participant_protocols == NULL || identity == NULL) {
        return LIVEKIT_CLIENT_PROTOCOL_DEFAULT;
    }
    khiter_t k = kh_get(participant_protocols, room->participant_protocols, identity);
    if (k == kh_end(room->participant_protocols)) {
        return LIVEKIT_CLIENT_PROTOCOL_DEFAULT;
    }
    return kh_value(room->participant_protocols, k);
}

/// Free every entry plus the map itself.
static void participant_registry_destroy(livekit_room_t *room)
{
    if (room->participant_protocols == NULL) {
        return;
    }
    for (khiter_t k = kh_begin(room->participant_protocols);
         k != kh_end(room->participant_protocols); ++k) {
        if (kh_exist(room->participant_protocols, k)) {
            free((void *)kh_key(room->participant_protocols, k));
        }
    }
    kh_destroy(participant_protocols, room->participant_protocols);
    room->participant_protocols = NULL;
}

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

static void on_eng_data_packet(livekit_pb_data_packet_t* packet,
                               const uint8_t *raw, size_t raw_len, void *ctx)
{
    livekit_room_t *room = (livekit_room_t *)ctx;
    switch (packet->which_value) {
        case LIVEKIT_PB_DATA_PACKET_USER_TAG:
            on_user_packet(&packet->value.user, packet->participant_identity, ctx);
            break;
        case LIVEKIT_PB_DATA_PACKET_RPC_REQUEST_TAG:
        case LIVEKIT_PB_DATA_PACKET_RPC_ACK_TAG:
        case LIVEKIT_PB_DATA_PACKET_RPC_RESPONSE_TAG:
            rpc_server_manager_handle_packet(room->rpc_server, packet);
            break;
        case LIVEKIT_PB_DATA_PACKET_STREAM_HEADER_TAG:
            data_stream_reader_handle_header(room->data_stream_reader,
                packet->value.stream_header, packet->participant_identity,
                raw, raw_len);
            break;
        case LIVEKIT_PB_DATA_PACKET_STREAM_CHUNK_TAG:
            data_stream_reader_handle_chunk(room->data_stream_reader,
                packet->value.stream_chunk);
            break;
        case LIVEKIT_PB_DATA_PACKET_STREAM_TRAILER_TAG:
            data_stream_reader_handle_trailer(room->data_stream_reader,
                packet->value.stream_trailer);
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

    // Maintain the participant registry first so any user-facing callback
    // below already sees the up-to-date state if it queries us.
    if (info->identity != NULL) {
        if (info->state == LIVEKIT_PB_PARTICIPANT_INFO_STATE_DISCONNECTED) {
            participant_registry_remove(room, info->identity);
            if (is_local) {
                free(room->local_identity);
                room->local_identity = NULL;
            }
        } else {
            // Only DEFAULT (0) and DATA_STREAM_RPC (1) are recognized; everything
            // else is treated as DEFAULT per the RPC v2 spec.
            int proto = info->client_protocol == LIVEKIT_CLIENT_PROTOCOL_DATA_STREAM_RPC
                ? LIVEKIT_CLIENT_PROTOCOL_DATA_STREAM_RPC
                : LIVEKIT_CLIENT_PROTOCOL_DEFAULT;
            participant_registry_set(room, info->identity, proto);
            if (is_local && room->local_identity == NULL) {
                room->local_identity = strdup(info->identity);
            }
        }
    }

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
        room->participant_protocols = kh_init(participant_protocols);
        if (room->participant_protocols == NULL) {
            ESP_LOGE(TAG, "Failed to allocate participant registry");
            ret = LIVEKIT_ERR_NO_MEM;
            break;
        }
        room->engine = engine_init(&eng_options);
        if (room->engine == NULL) {
            ESP_LOGE(TAG, "Failed to create engine");
            ret = LIVEKIT_ERR_ENGINE;
            break;
        }
        rpc_server_manager_options_t rpc_server_options = {
            .on_result = on_rpc_result,
            .send_packet = send_reliable_packet,
            .ctx = room
        };
        if (rpc_server_manager_create(&room->rpc_server, &rpc_server_options) != RPC_SERVER_MANAGER_ERR_NONE) {
            ESP_LOGE(TAG, "Failed to create RPC server manager");
            ret = LIVEKIT_ERR_OTHER;
            break;
        }
        if (data_stream_reader_create(&room->data_stream_reader) != DATA_STREAM_READER_ERR_NONE) {
            ESP_LOGE(TAG, "Failed to create data stream reader");
            ret = LIVEKIT_ERR_OTHER;
            break;
        }
        data_stream_writer_options_t writer_options = {
            .send_packet = send_reliable_packet,
            .ctx = room
        };
        if (data_stream_writer_create(&room->data_stream_writer, &writer_options) != DATA_STREAM_WRITER_ERR_NONE) {
            ESP_LOGE(TAG, "Failed to create data stream writer");
            ret = LIVEKIT_ERR_OTHER;
            break;
        }
        *handle = (livekit_room_handle_t)room;
        return LIVEKIT_ERR_NONE;
    } while (0);

    participant_registry_destroy(room);
    free(room->local_identity);
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
    data_stream_reader_destroy(room->data_stream_reader);
    data_stream_writer_destroy(room->data_stream_writer);
    participant_registry_destroy(room);
    free(room->local_identity);
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

    if (rpc_server_manager_register(room->rpc_server, method, handler) != RPC_SERVER_MANAGER_ERR_NONE) {
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

    if (rpc_server_manager_unregister(room->rpc_server, method) != RPC_SERVER_MANAGER_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to unregister RPC method '%s'", method);
        return LIVEKIT_ERR_INVALID_STATE;
    }
    return LIVEKIT_ERR_NONE;
}

livekit_err_t livekit_room_data_stream_topic_register(livekit_room_handle_t handle, const char* topic, const livekit_data_stream_handler_t* handler)
{
    if (handle == NULL || topic == NULL || handler == NULL || handler->on_recv == NULL) {
        return LIVEKIT_ERR_INVALID_ARG;
    }
    livekit_room_t *room = (livekit_room_t *)handle;

    data_stream_reader_err_t err = data_stream_reader_register(room->data_stream_reader, topic, handler);
    if (err != DATA_STREAM_READER_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to register data stream handler for topic '%s'", topic);
        return err == DATA_STREAM_READER_ERR_FULL ? LIVEKIT_ERR_NO_MEM : LIVEKIT_ERR_OTHER;
    }
    return LIVEKIT_ERR_NONE;
}

livekit_err_t livekit_room_data_stream_topic_unregister(livekit_room_handle_t handle, const char* topic)
{
    if (handle == NULL || topic == NULL) {
        return LIVEKIT_ERR_INVALID_ARG;
    }
    livekit_room_t *room = (livekit_room_t *)handle;

    data_stream_reader_err_t err = data_stream_reader_unregister(room->data_stream_reader, topic);
    if (err != DATA_STREAM_READER_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to unregister data stream handler for topic '%s'", topic);
        return LIVEKIT_ERR_OTHER;
    }
    return LIVEKIT_ERR_NONE;
}

livekit_err_t livekit_room_data_stream_open(livekit_room_handle_t handle, const livekit_data_stream_options_t *options, livekit_data_stream_handle_t *stream)
{
    if (handle == NULL || options == NULL || stream == NULL) {
        return LIVEKIT_ERR_INVALID_ARG;
    }
    livekit_room_t *room = (livekit_room_t *)handle;

    data_stream_writer_err_t err = data_stream_writer_open(room->data_stream_writer, options, stream);
    if (err != DATA_STREAM_WRITER_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to open data stream");
        return err == DATA_STREAM_WRITER_ERR_FULL ? LIVEKIT_ERR_NO_MEM : LIVEKIT_ERR_OTHER;
    }
    return LIVEKIT_ERR_NONE;
}

livekit_err_t livekit_room_data_stream_write(livekit_room_handle_t handle, livekit_data_stream_handle_t stream, const uint8_t *data, size_t size)
{
    if (handle == NULL || stream == NULL) {
        return LIVEKIT_ERR_INVALID_ARG;
    }
    livekit_room_t *room = (livekit_room_t *)handle;

    data_stream_writer_err_t err = data_stream_writer_write(room->data_stream_writer, stream, data, size);
    if (err != DATA_STREAM_WRITER_ERR_NONE) {
        return LIVEKIT_ERR_OTHER;
    }
    return LIVEKIT_ERR_NONE;
}

livekit_err_t livekit_room_data_stream_close(livekit_room_handle_t handle, livekit_data_stream_handle_t stream)
{
    if (handle == NULL || stream == NULL) {
        return LIVEKIT_ERR_INVALID_ARG;
    }
    livekit_room_t *room = (livekit_room_t *)handle;

    data_stream_writer_err_t err = data_stream_writer_close(room->data_stream_writer, stream);
    if (err != DATA_STREAM_WRITER_ERR_NONE) {
        return LIVEKIT_ERR_OTHER;
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