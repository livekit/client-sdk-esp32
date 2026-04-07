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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/// Maximum size in bytes of a single data stream chunk.
#define LIVEKIT_DATA_STREAM_CHUNK_SIZE 15000

#ifdef __cplusplus
extern "C" {
#endif

/// Opaque handle to an open outgoing data stream.
/// @ingroup DataStreams
typedef void *livekit_data_stream_t;

/// Header information about a data stream.
/// @ingroup DataStreams
typedef struct {
    const char* stream_id;
    const char* topic;
    const char* sender_identity;
    int64_t timestamp;
    uint64_t total_length;
    bool has_total_length;
    /// True if this is a text stream (UTF-8 chunks), false if byte stream (raw binary).
    bool is_text;
} livekit_data_stream_header_t;

/// A received chunk of data stream content.
/// @ingroup DataStreams
typedef struct {
    const char* stream_id;
    uint64_t chunk_index;
    const uint8_t* content;
    size_t content_size;
} livekit_data_stream_chunk_t;

/// Trailer information when a data stream closes.
/// @ingroup DataStreams
typedef struct {
    const char* stream_id;
    /// Empty string for normal closure, non-empty for abnormal end.
    const char* reason;
} livekit_data_stream_trailer_t;

/// Called when a new data stream is opened.
/// @ingroup DataStreams
typedef void (*livekit_data_stream_open_cb_t)(
    const livekit_data_stream_header_t* header, void* ctx);

/// Called for each received chunk of a data stream.
/// @ingroup DataStreams
typedef void (*livekit_data_stream_recv_cb_t)(
    const livekit_data_stream_chunk_t* chunk, void* ctx);

/// Called when a data stream is closed.
/// @ingroup DataStreams
typedef void (*livekit_data_stream_close_cb_t)(
    const livekit_data_stream_trailer_t* trailer, void* ctx);

/// Handler for incoming data streams on a topic.
/// @ingroup DataStreams
typedef struct {
    /// Callback invoked for each received chunk. Required.
    livekit_data_stream_recv_cb_t on_recv;

    /// Callback invoked when a new stream is opened. Optional, can be NULL.
    livekit_data_stream_open_cb_t on_open;

    /// Callback invoked when a stream is closed. Optional, can be NULL.
    livekit_data_stream_close_cb_t on_close;

    /// User context passed to all callbacks.
    void* ctx;
} livekit_data_stream_handler_t;

#ifdef __cplusplus
}
#endif
