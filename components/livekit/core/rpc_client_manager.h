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
#include "protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *rpc_client_manager_handle_t;

typedef enum {
    RPC_CLIENT_MANAGER_ERR_NONE          =  0,
    RPC_CLIENT_MANAGER_ERR_INVALID_ARG   = -1,
    RPC_CLIENT_MANAGER_ERR_NO_MEM        = -2,
    RPC_CLIENT_MANAGER_ERR_INVALID_STATE = -3,
    RPC_CLIENT_MANAGER_ERR_SEND_FAILED   = -4,
} rpc_client_manager_err_t;

typedef struct {
    /// Send a data packet synchronously. Returns true on success.
    bool (*send_packet)(const livekit_pb_data_packet_t *packet, void *ctx);

    /// Return the @c client_protocol value advertised by a remote
    /// participant, or @ref LIVEKIT_CLIENT_PROTOCOL_DEFAULT if the
    /// participant is unknown.
    int (*get_peer_protocol)(const char *identity, void *ctx);

    /// Deliver an RPC invocation result back to the user.
    void (*on_result)(const livekit_rpc_result_t *result, void *ctx);

    /// Context pointer forwarded to every callback above.
    void *ctx;
} rpc_client_manager_options_t;

/// Create a new RPC client manager.
rpc_client_manager_err_t rpc_client_manager_create(
    rpc_client_manager_handle_t *handle,
    const rpc_client_manager_options_t *options);

/// Destroy the manager, failing any in-flight requests as
/// @ref LIVEKIT_RPC_RESULT_RECIPIENT_DISCONNECTED.
rpc_client_manager_err_t rpc_client_manager_destroy(
    rpc_client_manager_handle_t handle);

/// Issue a new RPC invocation. The result is delivered asynchronously
/// through the @c on_result callback in @ref rpc_client_manager_options_t,
/// except for early-failure conditions (e.g. oversized v1 payload) which
/// are delivered synchronously from within this call.
rpc_client_manager_err_t rpc_client_manager_invoke(
    rpc_client_manager_handle_t handle,
    const livekit_rpc_invoke_options_t *options);

/// Dispatch an incoming RPC packet (ack or response) to the pending
/// request that matches its request_id. Unmatched packets are dropped.
void rpc_client_manager_handle_packet(
    rpc_client_manager_handle_t handle,
    const livekit_pb_data_packet_t *packet);

/// Fail any pending requests targeted at @p identity with
/// @ref LIVEKIT_RPC_RESULT_RECIPIENT_DISCONNECTED. Called by the room
/// layer when a participant disconnects.
void rpc_client_manager_on_participant_disconnect(
    rpc_client_manager_handle_t handle,
    const char *identity);

#ifdef __cplusplus
}
#endif
