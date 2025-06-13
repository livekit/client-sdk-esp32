
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
    LIVEKIT_PEER_ERR_RTC            = -4,
    LIVEKIT_PEER_ERR_MESSAGE        = -5
} livekit_peer_err_t;


/// @brief Options for creating a peer.
typedef struct {
    /// @brief Whether the peer is a publisher or subscriber.
    livekit_pb_signal_target_t target;

    /// @brief Invoked when an SDP message is available. This can be either
    /// an offer or answer depending on target configuration.
    void (*on_sdp)(const char *sdp, void *ctx);

    /// @brief Invoked when a new ICE candidate is available.
    void (*on_ice_candidate)(const char *candidate, void *ctx);

    /// @brief Invoked when a data packet is received over the data channel.
    void (*on_packet_received)(livekit_pb_data_packet_t* packet, void *ctx);

    /// @brief Context pointer passed to the handlers.
    void *ctx;
} livekit_peer_options_t;

/// @brief Options for connecting a peer.
typedef struct {
    /// @brief Weather to force the use of relay ICE candidates.
    bool force_relay;

    /// @brief Media options used for creating SDP messages.
    livekit_eng_media_options_t* media;
} livekit_peer_connect_options_t;

livekit_peer_err_t livekit_peer_create(livekit_peer_handle_t *handle, livekit_peer_options_t options);
livekit_peer_err_t livekit_peer_destroy(livekit_peer_handle_t handle);

livekit_peer_err_t livekit_peer_connect(livekit_peer_handle_t handle, livekit_peer_connect_options_t options);
livekit_peer_err_t livekit_peer_disconnect(livekit_peer_handle_t handle);

/// @brief Sets the ICE server to use for the connection
/// @note Must be called prior to establishing the connection.
livekit_peer_err_t livekit_peer_set_ice_servers(livekit_peer_handle_t handle, esp_peer_ice_server_cfg_t *servers, int count);

/// @brief Handles an SDP message from the remote peer.
livekit_peer_err_t livekit_peer_handle_sdp(livekit_peer_handle_t handle, const char *sdp);

/// @brief Handles an ICE candidate from the remote peer.
livekit_peer_err_t livekit_peer_handle_ice_candidate(livekit_peer_handle_t handle, const char *candidate);

/// @brief Sends a data packet to the remote peer.
livekit_peer_err_t livekit_peer_send_data_packet(livekit_peer_handle_t handle, livekit_pb_data_packet_t* packet, livekit_pb_data_packet_kind_t kind);

#ifdef __cplusplus
}
#endif