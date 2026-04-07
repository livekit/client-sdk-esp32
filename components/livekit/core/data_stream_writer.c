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
#include "data_stream_writer.h"
#include "utils.h"

static const char* TAG = "livekit_data_stream_writer";

#define CHUNK_SIZE LIVEKIT_DATA_STREAM_CHUNK_SIZE

typedef struct data_stream_writer data_stream_writer_t;

typedef struct {
    bool active;
    char *topic;
    char stream_id[37];
    uint64_t chunk_index;
    data_stream_writer_t *writer;
} data_stream_write_descriptor_t;

struct data_stream_writer {
    data_stream_write_descriptor_t streams[CONFIG_LK_MAX_DATA_STREAMS];
    data_stream_writer_options_t options;
};

static data_stream_write_descriptor_t* find_empty_slot(data_stream_writer_t *w)
{
    for (int i = 0; i < CONFIG_LK_MAX_DATA_STREAMS; i++) {
        if (!w->streams[i].active) {
            return &w->streams[i];
        }
    }
    return NULL;
}

static bool send_packet(data_stream_writer_t *w, const livekit_pb_data_packet_t *packet)
{
    return w->options.send_packet(packet, w->options.ctx);
}

static data_stream_writer_err_t send_header(data_stream_write_descriptor_t *desc, const livekit_data_stream_options_t *options)
{
    livekit_pb_data_stream_header_t pb_header = LIVEKIT_PB_DATA_STREAM_HEADER_INIT_ZERO;
    strlcpy(pb_header.stream_id, desc->stream_id, sizeof(pb_header.stream_id));
    pb_header.timestamp = get_unix_time_ms();
    pb_header.topic = (char *)options->topic;
    pb_header.has_total_length = options->has_total_length;
    pb_header.total_length = options->total_length;

    if (options->is_text) {
        pb_header.which_content_header = LIVEKIT_PB_DATA_STREAM_HEADER_TEXT_HEADER_TAG;
    } else {
        pb_header.which_content_header = LIVEKIT_PB_DATA_STREAM_HEADER_BYTE_HEADER_TAG;
    }

    livekit_pb_data_packet_t packet = LIVEKIT_PB_DATA_PACKET_INIT_ZERO;
    packet.which_value = LIVEKIT_PB_DATA_PACKET_STREAM_HEADER_TAG;
    packet.value.stream_header = &pb_header;

    if (!send_packet(desc->writer, &packet)) {
        return DATA_STREAM_WRITER_ERR_SEND;
    }
    return DATA_STREAM_WRITER_ERR_NONE;
}

static data_stream_writer_err_t send_chunk(data_stream_write_descriptor_t *desc, const uint8_t *data, size_t size)
{
    pb_bytes_array_t *content = malloc(PB_BYTES_ARRAY_T_ALLOCSIZE(size));
    if (content == NULL) {
        return DATA_STREAM_WRITER_ERR_NO_MEM;
    }
    content->size = (pb_size_t)size;
    memcpy(content->bytes, data, size);

    livekit_pb_data_stream_chunk_t pb_chunk = LIVEKIT_PB_DATA_STREAM_CHUNK_INIT_ZERO;
    strlcpy(pb_chunk.stream_id, desc->stream_id, sizeof(pb_chunk.stream_id));
    pb_chunk.chunk_index = desc->chunk_index;
    pb_chunk.content = content;

    livekit_pb_data_packet_t packet = LIVEKIT_PB_DATA_PACKET_INIT_ZERO;
    packet.which_value = LIVEKIT_PB_DATA_PACKET_STREAM_CHUNK_TAG;
    packet.value.stream_chunk = &pb_chunk;

    bool ok = send_packet(desc->writer, &packet);
    free(content);

    if (!ok) {
        return DATA_STREAM_WRITER_ERR_SEND;
    }

    desc->chunk_index++;
    return DATA_STREAM_WRITER_ERR_NONE;
}

static data_stream_writer_err_t send_trailer(data_stream_write_descriptor_t *desc)
{
    livekit_pb_data_stream_trailer_t pb_trailer = LIVEKIT_PB_DATA_STREAM_TRAILER_INIT_ZERO;
    strlcpy(pb_trailer.stream_id, desc->stream_id, sizeof(pb_trailer.stream_id));

    livekit_pb_data_packet_t packet = LIVEKIT_PB_DATA_PACKET_INIT_ZERO;
    packet.which_value = LIVEKIT_PB_DATA_PACKET_STREAM_TRAILER_TAG;
    packet.value.stream_trailer = &pb_trailer;

    if (!send_packet(desc->writer, &packet)) {
        return DATA_STREAM_WRITER_ERR_SEND;
    }
    return DATA_STREAM_WRITER_ERR_NONE;
}

data_stream_writer_err_t data_stream_writer_create(data_stream_writer_handle_t *handle, const data_stream_writer_options_t *options)
{
    if (handle == NULL || options == NULL || options->send_packet == NULL) {
        return DATA_STREAM_WRITER_ERR_INVALID_ARG;
    }
    data_stream_writer_t *w = calloc(1, sizeof(data_stream_writer_t));
    if (w == NULL) {
        return DATA_STREAM_WRITER_ERR_NO_MEM;
    }
    w->options = *options;
    *handle = (data_stream_writer_handle_t)w;
    return DATA_STREAM_WRITER_ERR_NONE;
}

data_stream_writer_err_t data_stream_writer_destroy(data_stream_writer_handle_t handle)
{
    if (handle == NULL) {
        return DATA_STREAM_WRITER_ERR_INVALID_ARG;
    }
    data_stream_writer_t *w = (data_stream_writer_t *)handle;
    for (int i = 0; i < CONFIG_LK_MAX_DATA_STREAMS; i++) {
        free(w->streams[i].topic);
    }
    free(w);
    return DATA_STREAM_WRITER_ERR_NONE;
}

data_stream_writer_err_t data_stream_writer_open(data_stream_writer_handle_t handle, const livekit_data_stream_options_t *options, data_stream_t *stream)
{
    if (handle == NULL || options == NULL || options->topic == NULL || stream == NULL) {
        return DATA_STREAM_WRITER_ERR_INVALID_ARG;
    }
    data_stream_writer_t *w = (data_stream_writer_t *)handle;

    data_stream_write_descriptor_t *slot = find_empty_slot(w);
    if (slot == NULL) {
        ESP_LOGE(TAG, "No free stream slots");
        return DATA_STREAM_WRITER_ERR_FULL;
    }

    slot->topic = strdup(options->topic);
    if (slot->topic == NULL) {
        return DATA_STREAM_WRITER_ERR_NO_MEM;
    }
    slot->active = true;
    slot->chunk_index = 0;
    slot->writer = w;
    generate_uuid(slot->stream_id);

    data_stream_writer_err_t err = send_header(slot, options);
    if (err != DATA_STREAM_WRITER_ERR_NONE) {
        free(slot->topic);
        slot->topic = NULL;
        slot->active = false;
        return err;
    }

    *stream = (data_stream_t)slot;
    return DATA_STREAM_WRITER_ERR_NONE;
}

data_stream_writer_err_t data_stream_writer_write(data_stream_t stream, const uint8_t *data, size_t size)
{
    if (stream == NULL || (data == NULL && size > 0)) {
        return DATA_STREAM_WRITER_ERR_INVALID_ARG;
    }
    data_stream_write_descriptor_t *desc = (data_stream_write_descriptor_t *)stream;
    if (!desc->active) {
        return DATA_STREAM_WRITER_ERR_CLOSED;
    }

    const uint8_t *ptr = data;
    size_t remaining = size;
    while (remaining > 0) {
        size_t chunk_size = remaining > CHUNK_SIZE ? CHUNK_SIZE : remaining;
        data_stream_writer_err_t err = send_chunk(desc, ptr, chunk_size);
        if (err != DATA_STREAM_WRITER_ERR_NONE) {
            return err;
        }
        ptr += chunk_size;
        remaining -= chunk_size;
    }

    return DATA_STREAM_WRITER_ERR_NONE;
}

data_stream_writer_err_t data_stream_writer_close(data_stream_t stream)
{
    if (stream == NULL) {
        return DATA_STREAM_WRITER_ERR_INVALID_ARG;
    }
    data_stream_write_descriptor_t *desc = (data_stream_write_descriptor_t *)stream;
    if (!desc->active) {
        return DATA_STREAM_WRITER_ERR_CLOSED;
    }

    data_stream_writer_err_t err = send_trailer(desc);
    desc->active = false;
    free(desc->topic);
    desc->topic = NULL;
    desc->stream_id[0] = '\0';
    desc->chunk_index = 0;
    return err;
}
