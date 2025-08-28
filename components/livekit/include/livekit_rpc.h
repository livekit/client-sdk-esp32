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

#ifdef __cplusplus
extern "C" {
#endif

/// Maximum payload size for RPC messages.
/// @ingroup RPC
#define LIVEKIT_RPC_MAX_PAYLOAD_BYTES 15360 // 15 KB

/// Built-in RPC error codes.
typedef enum {
    /// The RPC method returned normally.
    LIVEKIT_RPC_RESULT_OK = 0,

    /// Application error in method handler.
    LIVEKIT_RPC_RESULT_APPLICATION = 1500,

    /// Connection timeout.
    LIVEKIT_RPC_RESULT_CONNECTION_TIMEOUT = 1501,

    /// Response timeout.
    LIVEKIT_RPC_RESULT_RESPONSE_TIMEOUT = 1502,

    /// Recipient disconnected.
    LIVEKIT_RPC_RESULT_RECIPIENT_DISCONNECTED = 1503,

    /// Response payload too large.
    LIVEKIT_RPC_RESULT_RESPONSE_PAYLOAD_TOO_LARGE = 1504,

    /// Failed to send.
    LIVEKIT_RPC_RESULT_SEND_FAILED = 1505,

    /// Method not supported at destination.
    LIVEKIT_RPC_RESULT_UNSUPPORTED_METHOD = 1400,

    /// Recipient not found.
    LIVEKIT_RPC_RESULT_RECIPIENT_NOT_FOUND = 1401,

    /// Request payload too large.
    LIVEKIT_RPC_RESULT_REQUEST_PAYLOAD_TOO_LARGE = 1402,

    /// RPC not supported by server.
    LIVEKIT_RPC_RESULT_UNSUPPORTED_SERVER = 1403,

    /// Unsupported RPC version.
    LIVEKIT_RPC_RESULT_UNSUPPORTED_VERSION = 1404
} livekit_rpc_result_code_t;

/// The result of an RPC method invocation.
typedef struct {
    /// Invocation identifier.
    char* id;

    /// The error code if the RPC method failed.
    /// @note The value @ref LIVEKIT_RPC_ERR_NONE indicates an ok result.
    livekit_rpc_result_code_t code;

    /// Optional, textual description of the error that occurred.
    char* error_message;

    /// Payload returned to the caller.
    char* payload;
} livekit_rpc_result_t;

/// Details about an RPC method invocation.
typedef struct {
    /// Invocation identifier.
    char* id;

    /// The name of the method being invoked.
    char* method;

    /// Participant identity of the caller.
    char* caller_identity;

    /// Caller provided payload.
    ///
    /// If no payload is provided, this field will be NULL. Otherwise,
    /// it is guaranteed to be a valid NULL-terminated string.
    ///
    char* payload;

    /// Sends the result of the invocation to the caller.
    bool (*send_result)(const livekit_rpc_result_t* res, void* ctx);

    /// Context for the callback.
    void *ctx;
} livekit_rpc_invocation_t;

/// Handler for an RPC invocation.
/// @ingroup RPC
typedef void (*livekit_rpc_handler_t)(const livekit_rpc_invocation_t* invocation, void* ctx);

/// Returns an ok result from an RPC handler.
/// @param _payload The payload to return to the caller.
/// @warning This macro is intended for use only in RPC handler methods, and expects the
/// invocation parameter to be named `invocation`.
#define livekit_rpc_return_ok(_payload) \
    invocation->send_result(&(livekit_rpc_result_t){ \
        .id = invocation->id, \
        .code = LIVEKIT_RPC_RESULT_OK, \
        .payload = (_payload), \
        .error_message = NULL \
    }, invocation->ctx)

/// Returns an error result from an RPC handler.
/// @param error_message The error message or NULL.
/// @warning This macro is intended for use only in RPC handler methods, and expects the
/// invocation parameter to be named `invocation`.
#define livekit_rpc_return_error(_error_message) \
    invocation->send_result(&(livekit_rpc_result_t){ \
        .id = invocation->id, \
        .code = LIVEKIT_RPC_RESULT_APPLICATION, \
        .payload = NULL, \
        .error_message = (_error_message) \
    }, invocation->ctx);

#ifdef __cplusplus
}
#endif