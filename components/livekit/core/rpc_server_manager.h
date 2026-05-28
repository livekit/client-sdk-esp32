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

#include "livekit_rpc.h"
#include "livekit_data_stream.h"
#include "protocol.h"
#include "data_stream_writer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *rpc_server_manager_handle_t;

typedef enum {
    RPC_SERVER_MANAGER_ERR_NONE           =  0,
    RPC_SERVER_MANAGER_ERR_INVALID_ARG    = -1,
    RPC_SERVER_MANAGER_ERR_NO_MEM         = -2,
    RPC_SERVER_MANAGER_ERR_INVALID_STATE  = -3,
    RPC_SERVER_MANAGER_ERR_SEND_FAILED    = -4,
    RPC_SERVER_MANAGER_ERR_REGISTRATION   = -5,
} rpc_server_manager_err_t;

typedef struct {
    void (*on_result)(const livekit_rpc_result_t* result, void* ctx);
    bool (*send_packet)(const livekit_pb_data_packet_t* packet, void *ctx);

    /// Data stream writer the manager uses to send v2 success responses
    /// on topic "lk.rpc_response". May be NULL to disable v2 transport
    /// (every response is then sent as a v1 RpcResponse packet).
    data_stream_writer_handle_t writer;

    void* ctx;
} rpc_server_manager_options_t;

/// Creates a new RPC manager.
rpc_server_manager_err_t rpc_server_manager_create(rpc_server_manager_handle_t *handle, const rpc_server_manager_options_t *options);

/// Destroys an RPC manager.
rpc_server_manager_err_t rpc_server_manager_destroy(rpc_server_manager_handle_t handle);

/// Registers a handler for an RPC method.
rpc_server_manager_err_t rpc_server_manager_register(rpc_server_manager_handle_t handle, const char* method, livekit_rpc_handler_t handler);

/// Unregisters a handler for an RPC method.
rpc_server_manager_err_t rpc_server_manager_unregister(rpc_server_manager_handle_t handle, const char* method);

/// Handles an incoming RPC packet.
rpc_server_manager_err_t rpc_server_manager_handle_packet(rpc_server_manager_handle_t handle, const livekit_pb_data_packet_t* packet);

/// Forward an incoming request data stream (topic "lk.rpc_request") header.
/// The manager extracts the request_id / method / version / timeout from
/// the header's attributes, immediately sends a RpcAck packet, and binds
/// the stream so its chunks accumulate the request payload.
void rpc_server_manager_on_request_stream_open(
    rpc_server_manager_handle_t handle,
    const livekit_data_stream_header_t *header);

/// Forward an incoming request stream chunk. The manager looks up the
/// bound in-flight request by stream id and appends the bytes.
void rpc_server_manager_on_request_stream_chunk(
    rpc_server_manager_handle_t handle,
    const livekit_data_stream_chunk_t *chunk);

/// Forward an incoming request stream close. The manager looks up the
/// bound in-flight request by stream id, dispatches to the registered
/// method handler, and arranges for the response to be sent as either a
/// v2 data stream (success) or a v1 RpcResponse packet (error).
void rpc_server_manager_on_request_stream_close(
    rpc_server_manager_handle_t handle,
    const livekit_data_stream_trailer_t *trailer);

#ifdef __cplusplus
}
#endif