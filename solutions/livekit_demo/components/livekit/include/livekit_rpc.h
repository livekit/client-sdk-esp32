
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// @brief Maximum payload size for RPC messages.
#define LIVEKIT_RPC_MAX_PAYLOAD_BYTES 15360 // 15 KB

/// @brief Built-in RPC error codes.
typedef enum {
    /// @brief The RPC method returned normally.
    LIVEKIT_RPC_RESULT_OK = 0,

    /// @brief Application error in method handler.
    LIVEKIT_RPC_RESULT_APPLICATION = 1500,

    /// @brief Connection timeout.
    LIVEKIT_RPC_RESULT_CONNECTION_TIMEOUT = 1501,

    /// @brief Response timeout.
    LIVEKIT_RPC_RESULT_RESPONSE_TIMEOUT = 1502,

    /// @brief Recipient disconnected.
    LIVEKIT_RPC_RESULT_RECIPIENT_DISCONNECTED = 1503,

    /// @brief Response payload too large.
    LIVEKIT_RPC_RESULT_RESPONSE_PAYLOAD_TOO_LARGE = 1504,

    /// @brief Failed to send.
    LIVEKIT_RPC_RESULT_SEND_FAILED = 1505,

    /// @brief Method not supported at destination.
    LIVEKIT_RPC_RESULT_UNSUPPORTED_METHOD = 1400,

    /// @brief Recipient not found.
    LIVEKIT_RPC_RESULT_RECIPIENT_NOT_FOUND = 1401,

    /// @brief Request payload too large.
    LIVEKIT_RPC_RESULT_REQUEST_PAYLOAD_TOO_LARGE = 1402,

    /// @brief RPC not supported by server.
    LIVEKIT_RPC_RESULT_UNSUPPORTED_SERVER = 1403,

    /// @brief Unsupported RPC version.
    LIVEKIT_RPC_RESULT_UNSUPPORTED_VERSION = 1404
} livekit_rpc_result_code_t;

/// @brief The result of an RPC method invocation.
typedef struct {
    /// @brief Invocation identifier.
    char* id;

    /// @brief The error code if the RPC method failed.
    /// @note The value LIVEKIT_RPC_ERR_NONE indicates an ok result.
    livekit_rpc_result_code_t code;

    /// @brief Optional, textual description of the error that occurred.
    char* error_message;

    /// @brief Payload returned to the caller.
    char* payload;
} livekit_rpc_result_t;

/// @brief Details about an RPC method invocation.
typedef struct {
    /// @brief Invocation identifier.
    char* id;

    /// @brief The name of the method being invoked.
    char* method;

    /// @brief Participant identity of the caller.
    char* caller_identity;

    /// @brief Caller provided payload.
    /// @note The payload must be less than or equal to LIVEKIT_RPC_MAX_PAYLOAD_BYTES bytes.
    char* payload;

    /// @brief Sends the result of the invocation to the caller.
    bool (*send_result)(const livekit_rpc_result_t* res, void* ctx);

    /// @brief Context for the callback.
    void *ctx;
} livekit_rpc_invocation_t;

/// @brief Handler for an RPC invocation.
typedef void (*livekit_rpc_handler_t)(const livekit_rpc_invocation_t* invocation, void* ctx);

/// @brief Returns an ok result from an RPC handler.
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

/// @brief Returns an error result from an RPC handler.
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