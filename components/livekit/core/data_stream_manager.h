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

typedef void *data_stream_manager_handle_t;

typedef enum {
    DATA_STREAM_MANAGER_ERR_NONE          =  0, ///< No error
    DATA_STREAM_MANAGER_ERR_INVALID_ARG   = -1, ///< Invalid argument (NULL handle, missing on_recv, etc.)
    DATA_STREAM_MANAGER_ERR_NO_MEM        = -2, ///< Dynamic memory allocation failed
    DATA_STREAM_MANAGER_ERR_FULL          = -3, ///< No free descriptor slots available
} data_stream_manager_err_t;

/// Creates a new data stream manager.
data_stream_manager_err_t data_stream_manager_create(data_stream_manager_handle_t *handle);

/// Destroys a data stream manager.
data_stream_manager_err_t data_stream_manager_destroy(data_stream_manager_handle_t handle);

/// Registers a handler for a topic.
data_stream_manager_err_t data_stream_manager_register(data_stream_manager_handle_t handle, const char* topic, const livekit_data_stream_handler_t* handler);

/// Unregisters a handler for a topic. Clears the entire slot.
data_stream_manager_err_t data_stream_manager_unregister(data_stream_manager_handle_t handle, const char* topic);

/// Handles an incoming stream header.
data_stream_manager_err_t data_stream_manager_handle_header(data_stream_manager_handle_t handle, const livekit_pb_data_stream_header_t* header, const char* sender_identity);

/// Handles an incoming stream chunk.
data_stream_manager_err_t data_stream_manager_handle_chunk(data_stream_manager_handle_t handle, const livekit_pb_data_stream_chunk_t* chunk);

/// Handles an incoming stream trailer.
data_stream_manager_err_t data_stream_manager_handle_trailer(data_stream_manager_handle_t handle, const livekit_pb_data_stream_trailer_t* trailer);

#ifdef __cplusplus
}
#endif
