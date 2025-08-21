
#pragma once

#include "protocol.h"
#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *signal_handle_t;

typedef enum {
    SIGNAL_ERR_NONE        =  0,
    SIGNAL_ERR_INVALID_ARG = -1,
    SIGNAL_ERR_NO_MEM      = -2,
    SIGNAL_ERR_WEBSOCKET   = -3,
    SIGNAL_ERR_INVALID_URL = -4,
    SIGNAL_ERR_MESSAGE     = -5,
    SIGNAL_ERR_OTHER       = -6,
    // TODO: Add more error cases as needed
} signal_err_t;

/// Reason why signal connection failed.
typedef enum {
    /// No failure has occurred.
    SIGNAL_FAILURE_REASON_NONE = 0,

    /// Server unreachable.
    SIGNAL_FAILURE_REASON_UNREACHABLE  = 1 << 0,

    /// Token is malformed.
    SIGNAL_FAILURE_REASON_BAD_TOKEN    = 1 << 1,

    /// Token is not valid to join the room.
    SIGNAL_FAILURE_REASON_UNAUTHORIZED = 1 << 2,

    /// Other client error not covered by other reasons.
    SIGNAL_FAILURE_REASON_CLIENT_OTHER = 1 << 3,

    /// Any client error, no retry should be attempted.
    SIGNAL_FAILURE_REASON_CLIENT_ANY   = SIGNAL_FAILURE_REASON_BAD_TOKEN |
                                         SIGNAL_FAILURE_REASON_UNAUTHORIZED |
                                         SIGNAL_FAILURE_REASON_CLIENT_OTHER,
    /// Internal server error.
    SIGNAL_FAILURE_REASON_INTERNAL     = 1 << 4
} signal_failure_reason_t;

typedef struct {
    void* ctx;

    /// Invoked when the connection state changes.
    void (*on_state_changed)(connection_state_t state, void *ctx);

    /// Invoked when a signal response is received.
    ///
    /// The receiver returns true to take ownership of the response. If
    /// ownership is not taken (false), the response will be freed with
    /// `protocol_signal_res_free` internally.
    ///
    bool (*on_res)(livekit_pb_signal_response_t *res, void *ctx);
} signal_options_t;

signal_err_t signal_create(signal_handle_t *handle, signal_options_t *options);
signal_err_t signal_destroy(signal_handle_t handle);

/// Establishes the WebSocket connection
/// @note This function will close the existing connection if already connected.
signal_err_t signal_connect(signal_handle_t handle, const char* server_url, const char* token);

/// Closes the WebSocket connection
signal_err_t signal_close(signal_handle_t handle);

/// Returns the reason why the connection failed.
///
/// Use after the signal client's state changes to `CONNECTION_STATE_FAILED`.
/// Will be reset to `SIGNAL_FAILURE_REASON_NONE` during the next connection attempt.
///
signal_failure_reason_t signal_get_failure_reason(signal_handle_t handle);

/// Sends a leave request.
signal_err_t signal_send_leave(signal_handle_t handle);
signal_err_t signal_send_offer(signal_handle_t handle, const char *sdp);
signal_err_t signal_send_answer(signal_handle_t handle, const char *sdp);

signal_err_t signal_send_add_track(signal_handle_t handle, livekit_pb_add_track_request_t *req);
signal_err_t signal_send_update_subscription(signal_handle_t handle, const char *sid, bool subscribe);

#ifdef __cplusplus
}
#endif