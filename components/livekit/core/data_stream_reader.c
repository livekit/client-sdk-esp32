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

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <esp_log.h>
#include "data_stream_reader.h"

static const char* TAG = "livekit_data_stream";

typedef struct {
    bool active;
    char* topic;
    livekit_data_stream_handler_t handler;
    char stream_id[37];
    uint64_t next_chunk_index;
    uint64_t bytes_processed;
    uint64_t total_length;
    bool has_total_length;
} data_stream_descriptor_t;

typedef struct {
    data_stream_descriptor_t streams[CONFIG_LK_MAX_DATA_STREAMS];
} data_stream_reader_t;

static void clear_descriptor(data_stream_descriptor_t *desc)
{
    free(desc->topic);
    memset(desc, 0, sizeof(data_stream_descriptor_t));
}

static void reset_stream_state(data_stream_descriptor_t *desc)
{
    desc->active = false;
    desc->stream_id[0] = '\0';
    desc->next_chunk_index = 0;
    desc->bytes_processed = 0;
    desc->total_length = 0;
    desc->has_total_length = false;
}

static data_stream_descriptor_t* find_empty_slot(data_stream_reader_t *mgr)
{
    for (int i = 0; i < CONFIG_LK_MAX_DATA_STREAMS; i++) {
        if (mgr->streams[i].handler.on_recv == NULL) {
            return &mgr->streams[i];
        }
    }
    return NULL;
}

static data_stream_descriptor_t* find_by_stream_id(data_stream_reader_t *mgr, const char* stream_id)
{
    for (int i = 0; i < CONFIG_LK_MAX_DATA_STREAMS; i++) {
        data_stream_descriptor_t *desc = &mgr->streams[i];
        if (desc->active && strcmp(desc->stream_id, stream_id) == 0) {
            return desc;
        }
    }
    return NULL;
}

static data_stream_descriptor_t* find_by_topic(data_stream_reader_t *mgr, const char* topic)
{
    if (topic == NULL) {
        return NULL;
    }
    for (int i = 0; i < CONFIG_LK_MAX_DATA_STREAMS; i++) {
        data_stream_descriptor_t *desc = &mgr->streams[i];
        if (desc->handler.on_recv != NULL && !desc->active &&
            desc->topic != NULL && strcmp(desc->topic, topic) == 0) {
            return desc;
        }
    }
    return NULL;
}

data_stream_reader_err_t data_stream_reader_create(data_stream_reader_handle_t *handle)
{
    if (handle == NULL) {
        return DATA_STREAM_READER_ERR_INVALID_ARG;
    }
    data_stream_reader_t *mgr = calloc(1, sizeof(data_stream_reader_t));
    if (mgr == NULL) {
        return DATA_STREAM_READER_ERR_NO_MEM;
    }
    *handle = (data_stream_reader_handle_t)mgr;
    return DATA_STREAM_READER_ERR_NONE;
}

data_stream_reader_err_t data_stream_reader_destroy(data_stream_reader_handle_t handle)
{
    if (handle == NULL) {
        return DATA_STREAM_READER_ERR_INVALID_ARG;
    }
    data_stream_reader_t *mgr = (data_stream_reader_t *)handle;
    for (int i = 0; i < CONFIG_LK_MAX_DATA_STREAMS; i++) {
        free(mgr->streams[i].topic);
    }
    free(mgr);
    return DATA_STREAM_READER_ERR_NONE;
}

data_stream_reader_err_t data_stream_reader_register(data_stream_reader_handle_t handle, const char* topic, const livekit_data_stream_handler_t* handler)
{
    if (handle == NULL || topic == NULL || handler == NULL || handler->on_recv == NULL) {
        return DATA_STREAM_READER_ERR_INVALID_ARG;
    }
    data_stream_reader_t *mgr = (data_stream_reader_t *)handle;

    data_stream_descriptor_t *slot = find_empty_slot(mgr);
    if (slot == NULL) {
        return DATA_STREAM_READER_ERR_FULL;
    }

    slot->topic = strdup(topic);
    if (slot->topic == NULL) {
        return DATA_STREAM_READER_ERR_NO_MEM;
    }
    slot->handler = *handler;
    return DATA_STREAM_READER_ERR_NONE;
}

data_stream_reader_err_t data_stream_reader_unregister(data_stream_reader_handle_t handle, const char* topic)
{
    if (handle == NULL || topic == NULL) {
        return DATA_STREAM_READER_ERR_INVALID_ARG;
    }
    data_stream_reader_t *mgr = (data_stream_reader_t *)handle;

    for (int i = 0; i < CONFIG_LK_MAX_DATA_STREAMS; i++) {
        data_stream_descriptor_t *desc = &mgr->streams[i];
        if (desc->handler.on_recv != NULL &&
            desc->topic != NULL && strcmp(desc->topic, topic) == 0) {
            clear_descriptor(desc);
            return DATA_STREAM_READER_ERR_NONE;
        }
    }
    return DATA_STREAM_READER_ERR_INVALID_ARG;
}

data_stream_reader_err_t data_stream_reader_handle_header(data_stream_reader_handle_t handle, const livekit_pb_data_stream_header_t* header, const char* sender_identity)
{
    if (handle == NULL || header == NULL) {
        return DATA_STREAM_READER_ERR_INVALID_ARG;
    }
    data_stream_reader_t *mgr = (data_stream_reader_t *)handle;

    if (find_by_stream_id(mgr, header->stream_id) != NULL) {
        ESP_LOGW(TAG, "Duplicate stream_id: %s", header->stream_id);
        return DATA_STREAM_READER_ERR_NONE;
    }

    data_stream_descriptor_t *slot = find_by_topic(mgr, header->topic);
    if (slot == NULL) {
        ESP_LOGD(TAG, "No handler for topic: %s", header->topic ? header->topic : "(null)");
        return DATA_STREAM_READER_ERR_NONE;
    }

    slot->active = true;
    strlcpy(slot->stream_id, header->stream_id, sizeof(slot->stream_id));
    slot->next_chunk_index = 0;
    slot->bytes_processed = 0;
    slot->total_length = header->total_length;
    slot->has_total_length = header->has_total_length;

    if (slot->handler.on_open != NULL) {
        livekit_data_stream_header_t info = {
            .stream_id = header->stream_id,
            .topic = header->topic,
            .sender_identity = sender_identity,
            .timestamp = header->timestamp,
            .total_length = header->total_length,
            .has_total_length = header->has_total_length,
            .is_text = header->which_content_header == LIVEKIT_PB_DATA_STREAM_HEADER_TEXT_HEADER_TAG,
        };
        slot->handler.on_open(&info, slot->handler.ctx);
    }

    return DATA_STREAM_READER_ERR_NONE;
}

data_stream_reader_err_t data_stream_reader_handle_chunk(data_stream_reader_handle_t handle, const livekit_pb_data_stream_chunk_t* chunk)
{
    if (handle == NULL || chunk == NULL) {
        return DATA_STREAM_READER_ERR_INVALID_ARG;
    }
    data_stream_reader_t *mgr = (data_stream_reader_t *)handle;

    data_stream_descriptor_t *desc = find_by_stream_id(mgr, chunk->stream_id);
    if (desc == NULL) {
        ESP_LOGD(TAG, "Unknown stream_id for chunk: %s", chunk->stream_id);
        return DATA_STREAM_READER_ERR_NONE;
    }

    if (chunk->chunk_index != desc->next_chunk_index) {
        ESP_LOGW(TAG, "Out-of-order chunk for stream %s: expected %" PRIu64 ", got %" PRIu64,
                 chunk->stream_id, desc->next_chunk_index, chunk->chunk_index);
    }

    size_t content_size = chunk->content != NULL ? chunk->content->size : 0;
    desc->bytes_processed += content_size;
    desc->next_chunk_index = chunk->chunk_index + 1;

    if (desc->has_total_length && desc->bytes_processed > desc->total_length) {
        ESP_LOGE(TAG, "Stream %s exceeded total_length", chunk->stream_id);
        reset_stream_state(desc);
        return DATA_STREAM_READER_ERR_NONE;
    }

    livekit_data_stream_chunk_t chunk_info = {
        .stream_id = chunk->stream_id,
        .chunk_index = chunk->chunk_index,
        .content = chunk->content != NULL ? chunk->content->bytes : NULL,
        .content_size = content_size,
    };
    desc->handler.on_recv(&chunk_info, desc->handler.ctx);

    return DATA_STREAM_READER_ERR_NONE;
}

data_stream_reader_err_t data_stream_reader_handle_trailer(data_stream_reader_handle_t handle, const livekit_pb_data_stream_trailer_t* trailer)
{
    if (handle == NULL || trailer == NULL) {
        return DATA_STREAM_READER_ERR_INVALID_ARG;
    }
    data_stream_reader_t *mgr = (data_stream_reader_t *)handle;

    data_stream_descriptor_t *desc = find_by_stream_id(mgr, trailer->stream_id);
    if (desc == NULL) {
        ESP_LOGD(TAG, "Unknown stream_id for trailer: %s", trailer->stream_id);
        return DATA_STREAM_READER_ERR_NONE;
    }

    if (trailer->reason[0] != '\0') {
        ESP_LOGW(TAG, "Stream %s closed abnormally: %s", trailer->stream_id, trailer->reason);
    }

    if (desc->handler.on_close != NULL) {
        livekit_data_stream_trailer_t trailer_info = {
            .stream_id = trailer->stream_id,
            .reason = trailer->reason,
        };
        desc->handler.on_close(&trailer_info, desc->handler.ctx);
    }

    reset_stream_state(desc);
    return DATA_STREAM_READER_ERR_NONE;
}
