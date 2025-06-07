
#pragma once

#include "livekit_common.h"
#include "livekit_engine.h"
#include "livekit_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *livekit_peer_handle_t;

typedef enum {
    LIVEKIT_PEER_ERR_NONE           =  0,
    LIVEKIT_PEER_ERR_INVALID_ARG    = -1,
    LIVEKIT_PEER_ERR_NO_MEM         = -2,
    LIVEKIT_PEER_ERR_INVALID_STATE  = -3,
    LIVEKIT_PEER_ERR_RTC            = -4
} livekit_peer_err_t;

typedef struct {
    livekit_signal_target_t target;
    void *ctx;
    void (*on_sdp)(const char *sdp, void *ctx);
    void (*on_ice_candidate)(const char *candidate, void *ctx);
} livekit_peer_options_t;

typedef struct {
    bool force_relay;
    livekit_eng_media_options_t* media;
} livekit_peer_connect_options_t;

livekit_peer_err_t livekit_peer_create(livekit_peer_handle_t *handle, livekit_peer_options_t options);
livekit_peer_err_t livekit_peer_destroy(livekit_peer_handle_t handle);

livekit_peer_err_t livekit_peer_connect(livekit_peer_handle_t handle, livekit_peer_connect_options_t options);
livekit_peer_err_t livekit_peer_disconnect(livekit_peer_handle_t handle);

/// @brief Sets the ICE server to use for the connection
/// @note Must be called prior to establishing the connection.
livekit_peer_err_t livekit_peer_set_ice_servers(livekit_peer_handle_t handle, livekit_ice_server_t *servers, int count);

/// @brief Handles an SDP message from the remote peer.
livekit_peer_err_t livekit_peer_handle_sdp(livekit_peer_handle_t handle, const char *sdp);

/// @brief Handles an ICE candidate from the remote peer.
livekit_peer_err_t livekit_peer_handle_ice_candidate(livekit_peer_handle_t handle, const char *candidate);

#ifdef __cplusplus
}
#endif