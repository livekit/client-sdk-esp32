
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

    /// LiveKit server could not be reached.
    ///
    /// This may occur due to network connectivity issues, incorrect URL,
    /// TLS handshake failure, or an offline server.
    ///
    LIVEKIT_FAILURE_REASON_UNREACHABLE,

    /// Token is malformed.
    ///
    /// This can occur if the token has missing/empty identity or room fields,
    /// or if either of these fields exceeds the maximum length.
    ///
    LIVEKIT_FAILURE_REASON_BAD_TOKEN,

    /// Token is not valid to join the room.
    ///
    /// This can be caused by an expired token, or a token that lacks
    /// necessary claims.
    ///
    LIVEKIT_FAILURE_REASON_UNAUTHORIZED,

    /// WebRTC establishment failure.
    ///
    /// Required peer connection(s) could not be established or failed.
    ///
    LIVEKIT_FAILURE_REASON_RTC,

    /// Maximum number of retries reached.
    ///
    /// Room connection failed after `CONFIG_LK_MAX_RETRIES` retries.
    ///
    LIVEKIT_FAILURE_REASON_MAX_RETRIES,

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