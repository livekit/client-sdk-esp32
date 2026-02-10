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

#include "common.h"
#include "protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *peer_handle_t;

typedef enum {
    PEER_ERR_NONE           =  0,
    PEER_ERR_INVALID_ARG    = -1,
    PEER_ERR_NO_MEM         = -2,
    PEER_ERR_INVALID_STATE  = -3,
    PEER_ERR_RTC            = -4,
    PEER_ERR_MESSAGE        = -5
} peer_err_t;

typedef enum {
    PEER_ROLE_PUBLISHER,
    PEER_ROLE_SUBSCRIBER
} peer_role_t;

/// Options for creating a peer.
typedef struct {
    /// Peer role (publisher or subscriber).
    peer_role_t role;

    /// ICE server list.
    esp_peer_ice_server_cfg_t* server_list;

    /// Number of servers in the list.
    uint8_t server_count;

    /// Weather to force the use of relay ICE candidates.
    bool force_relay;

    /// Media options used for creating SDP messages.
    engine_media_options_t* media;

    /// Invoked when the peer's connection state changes.
    void (*on_state_changed)(connection_state_t state, peer_role_t role, void *ctx);

    /// Invoked when a data packet is received over the data channel.
    ///
    /// The receiver returns true to take ownership of the packet. If
    /// ownership is not taken (false), the packet will be freed with
    /// `protocol_data_packet_free` internally.
    ///
    bool (*on_data_packet)(livekit_pb_data_packet_t* packet, void *ctx);

    /// Invoked when an SDP message is available. This can be either
    /// an offer or answer depending on target configuration.
    void (*on_sdp)(const char *sdp, peer_role_t role, void *ctx);

    /// Invoked when information about an incoming audio stream is available.
    void (*on_audio_info)(esp_peer_audio_stream_info_t* info, void *ctx);

    /// Invoked when an audio frame is received.
    void (*on_audio_frame)(esp_peer_audio_frame_t* frame, void *ctx);

    /// Invoked when information about an incoming video stream is available.
    void (*on_video_info)(esp_peer_video_stream_info_t* info, void *ctx);

    /// Invoked when a video frame is received.
    void (*on_video_frame)(esp_peer_video_frame_t* frame, void *ctx);

    /// Context pointer passed to the handlers.
    void *ctx;
} peer_options_t;

peer_err_t peer_create(peer_handle_t *handle, peer_options_t *options);
peer_err_t peer_destroy(peer_handle_t handle);

peer_err_t peer_connect(peer_handle_t handle);
peer_err_t peer_disconnect(peer_handle_t handle);

/// Handles an SDP message from the remote peer.
peer_err_t peer_handle_sdp(peer_handle_t handle, const char *sdp);

/// Handles an ICE candidate from the remote peer.
peer_err_t peer_handle_ice_candidate(peer_handle_t handle, const char *candidate);

/// Sends a data packet to the remote peer.
peer_err_t peer_send_data_packet(peer_handle_t handle, const livekit_pb_data_packet_t* packet, bool reliable);

/// Sends an audio frame to the remote peer.
/// @warning Only use on publisher peer.
peer_err_t peer_send_audio(peer_handle_t handle, esp_peer_audio_frame_t* frame);

/// Sends a video frame to the remote peer.
/// @warning Only use on publisher peer.
peer_err_t peer_send_video(peer_handle_t handle, esp_peer_video_frame_t* frame);

#ifdef __cplusplus
}
#endif