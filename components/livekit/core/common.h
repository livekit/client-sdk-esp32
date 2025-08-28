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

#include "esp_peer.h"
#include "esp_capture.h"
#include "av_render.h"

#ifdef __cplusplus
extern "C" {
#endif

/// State of a connection.
typedef enum {
    CONNECTION_STATE_DISCONNECTED = 0, /// Disconnected
    CONNECTION_STATE_CONNECTING   = 1, /// Establishing connection
    CONNECTION_STATE_CONNECTED    = 2, /// Connected
    CONNECTION_STATE_RECONNECTING = 3, /// Connection was previously established, but was lost
    CONNECTION_STATE_FAILED       = 4  /// Connection failed
} connection_state_t;

typedef struct {
    esp_peer_media_dir_t audio_dir;
    esp_peer_media_dir_t video_dir;

    esp_peer_audio_stream_info_t audio_info;
    esp_peer_video_stream_info_t video_info;

    esp_capture_handle_t capturer;
    av_render_handle_t   renderer;
} engine_media_options_t;

#ifdef __cplusplus
}
#endif
