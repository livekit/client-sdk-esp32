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

#include "protocol.h"

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

/// Signal connection state.
typedef enum {
    /// Disconnected.
    SIGNAL_STATE_DISCONNECTED        = 0,

    /// Establishing connection.
    SIGNAL_STATE_CONNECTING          = 1 << 0,

    /// Connection established.
    SIGNAL_STATE_CONNECTED           = 1 << 1,

    /// Server unreachable.
    SIGNAL_STATE_FAILED_UNREACHABLE  = 1 << 2,

    /// Server did not respond to ping within timeout window.
    SIGNAL_STATE_FAILED_PING_TIMEOUT = 1 << 3,

    /// Internal server error.
    SIGNAL_STATE_FAILED_INTERNAL     = 1 << 4,

    /// Token is malformed.
    SIGNAL_STATE_FAILED_BAD_TOKEN    = 1 << 5,

    /// Token is not valid to join the room.
    SIGNAL_STATE_FAILED_UNAUTHORIZED = 1 << 6,

    /// Other client failure not covered by other reasons.
    SIGNAL_STATE_FAILED_CLIENT_OTHER = 1 << 7,

    /// Any client failure (retry should not be attempted).
    SIGNAL_STATE_FAILED_CLIENT_ANY   = SIGNAL_STATE_FAILED_BAD_TOKEN    |
                                       SIGNAL_STATE_FAILED_UNAUTHORIZED |
                                       SIGNAL_STATE_FAILED_CLIENT_OTHER,

    /// Any failure.
    SIGNAL_STATE_FAILED_ANY          = SIGNAL_STATE_FAILED_UNREACHABLE  |
                                       SIGNAL_STATE_FAILED_PING_TIMEOUT |
                                       SIGNAL_STATE_FAILED_INTERNAL     |
                                       SIGNAL_STATE_FAILED_CLIENT_ANY
} signal_state_t;

typedef struct {
    void* ctx;

    /// Invoked when the connection state changes.
    void (*on_state_changed)(signal_state_t state, void *ctx);

    /// Invoked when a signal response is received.
    ///
    /// The receiver returns true to take ownership of the response. If
    /// ownership is not taken (false), the response will be freed with
    /// `protocol_signal_response_free` internally.
    ///
    bool (*on_res)(livekit_pb_signal_response_t *res, void *ctx);
} signal_options_t;

signal_handle_t signal_init(const signal_options_t *options);
signal_err_t signal_destroy(signal_handle_t handle);

/// Establishes the WebSocket connection
/// @note This function will close the existing connection if already connected.
signal_err_t signal_connect(signal_handle_t handle, const char* server_url, const char* token);

/// Closes the WebSocket connection
signal_err_t signal_close(signal_handle_t handle);

/// Sends a leave request.
signal_err_t signal_send_leave(signal_handle_t handle);
signal_err_t signal_send_offer(signal_handle_t handle, const char *sdp);
signal_err_t signal_send_answer(signal_handle_t handle, const char *sdp);

signal_err_t signal_send_add_track(signal_handle_t handle, livekit_pb_add_track_request_t *req);
signal_err_t signal_send_update_subscription(signal_handle_t handle, const char *sid, bool subscribe);

#ifdef __cplusplus
}
#endif