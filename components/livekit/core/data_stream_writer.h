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

#pragma once

#include "livekit_data_stream.h"
#include "protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *data_stream_writer_handle_t;

typedef enum {
    DATA_STREAM_WRITER_ERR_NONE          =  0,
    DATA_STREAM_WRITER_ERR_INVALID_ARG   = -1,
    DATA_STREAM_WRITER_ERR_NO_MEM        = -2,
    DATA_STREAM_WRITER_ERR_FULL          = -3, ///< No free stream slots available
    DATA_STREAM_WRITER_ERR_SEND          = -4, ///< Failed to send packet
    DATA_STREAM_WRITER_ERR_CLOSED        = -5, ///< Stream already closed
} data_stream_writer_err_t;

typedef struct {
    bool (*send_packet)(const livekit_pb_data_packet_t* packet, void *ctx);
    void *ctx;
} data_stream_writer_options_t;

/// Creates a new data stream writer.
data_stream_writer_err_t data_stream_writer_create(data_stream_writer_handle_t *handle, const data_stream_writer_options_t *options);

/// Destroys a data stream writer.
data_stream_writer_err_t data_stream_writer_destroy(data_stream_writer_handle_t handle);

/// Opens a new outgoing data stream.
///
/// Allocates a slot, generates a stream_id and timestamp, sends the header
/// packet, and returns a stream handle for subsequent write/close calls.
data_stream_writer_err_t data_stream_writer_open(data_stream_writer_handle_t handle, const livekit_data_stream_options_t *options, livekit_data_stream_handle_t *stream);

/// Writes data to an open stream.
///
/// Data is automatically chunked into pieces of LIVEKIT_DATA_STREAM_CHUNK_SIZE
/// bytes. Can be called multiple times.
data_stream_writer_err_t data_stream_writer_write(livekit_data_stream_handle_t stream, const uint8_t *data, size_t size);

/// Closes an open stream.
///
/// Sends the trailer packet and releases the slot.
data_stream_writer_err_t data_stream_writer_close(livekit_data_stream_handle_t stream);

#ifdef __cplusplus
}
#endif
