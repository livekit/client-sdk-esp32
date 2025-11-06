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

#include "livekit_types.h"
#include "common.h"
#include "protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Handle to an engine instance.
typedef void *engine_handle_t;

typedef enum {
    ENGINE_ERR_NONE        =  0,
    ENGINE_ERR_INVALID_ARG = -1,
    ENGINE_ERR_NO_MEM      = -2,
    ENGINE_ERR_SIGNALING   = -3,
    ENGINE_ERR_RTC         = -4,
    ENGINE_ERR_MEDIA       = -5,
    ENGINE_ERR_OTHER       = -6,
    ENGINE_ERR_MAX_SUB     = -7, // No more subscriptions allowed.
    // TODO: Add more error cases as needed
} engine_err_t;

/// WebRTC media provider
/// @note   Media player and capture system are created externally.
///         WebRTC will internally use the capture and player handle to capture media data and perform media playback.
typedef struct {
    esp_capture_handle_t capture; /*!< Capture system handle */
    av_render_handle_t   player;  /*!< Player handle */
} engine_media_provider_t;

typedef struct {
    void *ctx;
    void (*on_state_changed)(livekit_connection_state_t state, void *ctx);
    void (*on_data_packet)(livekit_pb_data_packet_t* packet, void *ctx);
    void (*on_room_info)(const livekit_pb_room_t* info, void *ctx);
    void (*on_participant_info)(const livekit_pb_participant_info_t* info, bool is_local, void *ctx);
    engine_media_options_t media;
} engine_options_t;

/// Creates a new instance.
engine_handle_t engine_init(const engine_options_t *options);

/// Destroys an instance.
/// @param[in] handle The handle to the instance to destroy.
engine_err_t engine_destroy(engine_handle_t handle);

/// Connect the engine.
engine_err_t engine_connect(engine_handle_t handle, const char* server_url, const char* token);

/// Close the engine.
engine_err_t engine_close(engine_handle_t handle);

/// Returns the reason why the engine connection failed.
livekit_failure_reason_t engine_get_failure_reason(engine_handle_t handle);

/// Sends a data packet to the remote peer.
engine_err_t engine_send_data_packet(engine_handle_t handle, const livekit_pb_data_packet_t* packet, bool reliable);

#ifdef __cplusplus
}
#endif
