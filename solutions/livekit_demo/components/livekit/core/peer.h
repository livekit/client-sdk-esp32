
#pragma once

#include "common.h"
#include "engine.h"
#include "protocol.h"

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

typedef enum {
    LIVEKIT_PEER_STATE_DISCONNECTED = 0, /*!< Disconnected */
    LIVEKIT_PEER_STATE_CONNECTING   = 1, /*!< Establishing peer connection */
    LIVEKIT_PEER_STATE_CONNECTED    = 2, /*!< Connected to peer & data channels open */
    LIVEKIT_PEER_STATE_FAILED       = 3  /*!< Connection failed */
} livekit_peer_state_t;

/// @brief Options for creating a peer.
typedef struct {
    /// @brief Whether the peer is a publisher or subscriber.
    livekit_pb_signal_target_t target;

    /// @brief ICE server list.
    esp_peer_ice_server_cfg_t* server_list;

    /// @brief Number of servers in the list.
    int server_count;

    /// @brief Weather to force the use of relay ICE candidates.
    bool force_relay;

    /// @brief Whether the peer is the primary peer.
    /// @note This determines which peer controls the data channels.
    bool is_primary;

    /// @brief Media options used for creating SDP messages.
    livekit_eng_media_options_t* media;

    /// @brief Invoked when the peer's connection state changes.
    void (*on_state_changed)(livekit_peer_state_t state, void *ctx);

    /// @brief Invoked when an SDP message is available. This can be either
    /// an offer or answer depending on target configuration.
    void (*on_sdp)(const char *sdp, void *ctx);

    /// @brief Invoked when a new ICE candidate is available.
    void (*on_ice_candidate)(const char *candidate, void *ctx);

    /// @brief Invoked when a data packet is received over the data channel.
    void (*on_packet_received)(livekit_pb_data_packet_t* packet, void *ctx);

    /// @brief Invoked when information about an incoming audio stream is available.
    void (*on_audio_info)(esp_peer_audio_stream_info_t* info, void *ctx);

    /// @brief Invoked when an audio frame is received.
    void (*on_audio_frame)(esp_peer_audio_frame_t* frame, void *ctx);

    /// @brief Invoked when information about an incoming video stream is available.
    void (*on_video_info)(esp_peer_video_stream_info_t* info, void *ctx);

    /// @brief Invoked when a video frame is received.
    void (*on_video_frame)(esp_peer_video_frame_t* frame, void *ctx);

    /// @brief Context pointer passed to the handlers.
    void *ctx;
} livekit_peer_options_t;

livekit_peer_err_t livekit_peer_create(livekit_peer_handle_t *handle, livekit_peer_options_t *options);
livekit_peer_err_t livekit_peer_destroy(livekit_peer_handle_t handle);

livekit_peer_err_t livekit_peer_connect(livekit_peer_handle_t handle);
livekit_peer_err_t livekit_peer_disconnect(livekit_peer_handle_t handle);

/// @brief Handles an SDP message from the remote peer.
livekit_peer_err_t livekit_peer_handle_sdp(livekit_peer_handle_t handle, const char *sdp);

/// @brief Handles an ICE candidate from the remote peer.
livekit_peer_err_t livekit_peer_handle_ice_candidate(livekit_peer_handle_t handle, const char *candidate);

/// @brief Sends a data packet to the remote peer.
livekit_peer_err_t livekit_peer_send_data_packet(livekit_peer_handle_t handle, livekit_pb_data_packet_t* packet, livekit_pb_data_packet_kind_t kind);

/// @brief Sends an audio frame to the remote peer.
/// @warning Only use on publisher peer.
livekit_peer_err_t livekit_peer_send_audio(livekit_peer_handle_t handle, esp_peer_audio_frame_t* frame);

/// @brief Sends a video frame to the remote peer.
/// @warning Only use on publisher peer.
livekit_peer_err_t livekit_peer_send_video(livekit_peer_handle_t handle, esp_peer_video_frame_t* frame);

#ifdef __cplusplus
}
#endif