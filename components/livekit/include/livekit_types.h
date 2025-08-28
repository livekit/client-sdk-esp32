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

#ifdef __cplusplus
extern "C" {
#endif

/// Connection state of a room.
/// @ingroup Connection
typedef enum {
    LIVEKIT_CONNECTION_STATE_DISCONNECTED = 0, ///< Disconnected
    LIVEKIT_CONNECTION_STATE_CONNECTING   = 1, ///< Establishing connection
    LIVEKIT_CONNECTION_STATE_CONNECTED    = 2, ///< Connected
    LIVEKIT_CONNECTION_STATE_RECONNECTING = 3, ///< Reestablishing connection after a failure
    LIVEKIT_CONNECTION_STATE_FAILED       = 4  ///< Connection failed after maximum number of retries
} livekit_connection_state_t;

/// Reason why room connection failed.
/// @ingroup Connection
typedef enum {
    /// No failure has occurred.
    LIVEKIT_FAILURE_REASON_NONE,

    /// Unreachable
    ///
    /// LiveKit server could not be reached: this may occur due to network
    /// connectivity issues, incorrect URL, TLS handshake failure, or an offline server.
    ///
    LIVEKIT_FAILURE_REASON_UNREACHABLE,

    /// Bad Token
    ///
    /// Token is malformed: this can occur if the token has missing/empty identity
    /// or room fields, or if either of these fields exceeds the maximum length.
    ///
    LIVEKIT_FAILURE_REASON_BAD_TOKEN,

    /// Unauthorized
    ///
    /// Token is not valid to join the room: this can be caused by an
    /// expired token, or a token that lacks necessary claims.
    ///
    LIVEKIT_FAILURE_REASON_UNAUTHORIZED,

    /// RTC
    ///
    /// WebRTC establishment failure: required peer connection(s) could
    /// not be established or failed.
    ///
    LIVEKIT_FAILURE_REASON_RTC,

    /// Max Retries
    ///
    /// Maximum number of retries reached: room connection failed after
    /// `CONFIG_LK_MAX_RETRIES` retries.
    ///
    LIVEKIT_FAILURE_REASON_MAX_RETRIES,

    /// Ping Timeout
    ///
    /// Server did not respond to ping within the timeout window.
    ///
    LIVEKIT_FAILURE_REASON_PING_TIMEOUT,

    /// Duplicate Identity
    ///
    /// Another participant already has the same identity.
    ///
    /// Protocol equivalent: `DisconnectReason.DUPLICATE_IDENTITY`.
    ///
    LIVEKIT_FAILURE_REASON_DUPLICATE_IDENTITY,

    /// Server Shutdown
    ///
    /// LiveKit server instance is shutting down.
    ///
    /// Protocol equivalent: `DisconnectReason.SERVER_SHUTDOWN`.
    ///
    LIVEKIT_FAILURE_REASON_SERVER_SHUTDOWN,

    /// Participant Removed
    ///
    /// Participant was removed using room services API.
    ///
    /// Protocol equivalent: `DisconnectReason.PARTICIPANT_REMOVED`.
    ///
    LIVEKIT_FAILURE_REASON_PARTICIPANT_REMOVED,

    /// Room Deleted
    ///
    /// Room was deleted using room services API.
    ///
    /// Protocol equivalent: `DisconnectReason.ROOM_DELETED`.
    ///
    LIVEKIT_FAILURE_REASON_ROOM_DELETED,

    /// State Mismatch
    ///
    /// Client attempted to resume, but server is not aware of it.
    ///
    /// Protocol equivalent: `DisconnectReason.STATE_MISMATCH`.
    ///
    LIVEKIT_FAILURE_REASON_STATE_MISMATCH,

    /// Join Incomplete
    ///
    /// Client was unable to fully establish a connection.
    ///
    /// Protocol equivalent: `DisconnectReason.JOIN_FAILURE`.
    ///
    LIVEKIT_FAILURE_REASON_JOIN_INCOMPLETE,

    /// Migration
    ///
    /// The server requested the client to migrate the connection elsewhere (cloud only).
    ///
    /// Protocol equivalent: `DisconnectReason.MIGRATION`.
    ///
    LIVEKIT_FAILURE_REASON_MIGRATION,

    /// Signal Close
    ///
    /// The signal connection was closed unexpectedly.
    ///
    /// Protocol equivalent: `DisconnectReason.SIGNAL_CLOSE`.
    ///
    LIVEKIT_FAILURE_REASON_SIGNAL_CLOSE,

    /// Room Closed
    ///
    /// The room was closed, due to all Standard and Ingress participants having left.
    ///
    /// Protocol equivalent: `DisconnectReason.ROOM_CLOSED`.
    ///
    LIVEKIT_FAILURE_REASON_ROOM_CLOSED,

    /// SIP User Unavailable
    ///
    /// SIP callee did not respond in time.
    ///
    /// Protocol equivalent: `DisconnectReason.USER_UNAVAILABLE`.
    ///
    LIVEKIT_FAILURE_REASON_SIP_USER_UNAVAILABLE,

    /// SIP User Rejected
    ///
    /// SIP callee rejected the call (busy).
    ///
    /// Protocol equivalent: `DisconnectReason.USER_REJECTED`.
    ///
    LIVEKIT_FAILURE_REASON_SIP_USER_REJECTED,

    /// SIP Trunk Failure
    ///
    /// SIP protocol failure or unexpected response.
    ///
    /// Protocol equivalent: `DisconnectReason.SIP_TRUNK_FAILURE`.
    ///
    LIVEKIT_FAILURE_REASON_SIP_TRUNK_FAILURE,

    /// Connection Timeout
    ///
    /// Server timed out a participant session.
    ///
    /// Protocol equivalent: `DisconnectReason.CONNECTION_TIMEOUT`.
    ///
    LIVEKIT_FAILURE_REASON_CONNECTION_TIMEOUT,

    /// Media Failure
    ///
    /// Media stream failure or media timeout.
    ///
    /// Protocol equivalent: `DisconnectReason.MEDIA_FAILURE`.
    ///
    LIVEKIT_FAILURE_REASON_MEDIA_FAILURE,

    /// Other failure reason.
    ///
    /// Any other failure not covered by other reasons. Check console output
    /// for more details, and please report the issue on GitHub.
    ///
    LIVEKIT_FAILURE_REASON_OTHER
} livekit_failure_reason_t;

#ifdef __cplusplus
}
#endif