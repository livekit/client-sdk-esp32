/*
 * Copyright 2026 LiveKit, Inc.
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

#include <stdbool.h>

#include "esp_codec_dev.h"
#include "esp_capture_audio_src_if.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Configuration name for the ES8389-based Korvo board.
#define LIVEKIT_S31_BOARD_KORVO_1 "ESP32_S31_KORVO_1"
/// Configuration name for the ES8311-based Function-CoreBoard.
#define LIVEKIT_S31_BOARD_FUNCTION_COREBOARD_1 \
    "ESP32_S31_FUNCTION_COREBOARD_1"

/// Hardware selected by the singleton board adapter.
typedef enum {
    /// Board initialization has not succeeded.
    LIVEKIT_S31_BOARD_NONE = 0,
    /// Korvo-1 with stereo speaker connectors and onboard camera.
    LIVEKIT_S31_BOARD_KORVO,
    /// Function-CoreBoard-1 with mono speaker and no onboard camera.
    LIVEKIT_S31_BOARD_FUNCTION_COREBOARD,
} livekit_s31_board_type_t;

/// Initialize audio and the RGB status LED for a supported ESP32-S31 board.
esp_err_t livekit_s31_board_init(const char *board_name);

/// Return the selected board type after successful initialization.
livekit_s31_board_type_t livekit_s31_board_get_type(void);

/// Return the selected board's human-readable product name.
const char *livekit_s31_board_get_name(void);

/// Return the codec device used for speaker rendering.
esp_codec_dev_handle_t livekit_s31_board_get_playback_handle(void);

/// Return the codec device used for microphone capture.
esp_codec_dev_handle_t livekit_s31_board_get_record_handle(void);

/// Return the physical playback channel count exposed by the board profile.
unsigned livekit_s31_board_get_playback_channels(void);

/// Configure the singleton board capture source before starting it.
/// Keep native 48 kHz stereo timing; without AEC, duplicate Korvo's left slot
/// before downstream mono conversion. Do not configure concurrent sources.
esp_err_t livekit_s31_board_configure_capture(esp_capture_audio_src_if_t *source,
                                            bool use_aec);

/// Initialize the selected board's camera and register its V4L2 capture device.
///
/// ESP32-S31-Korvo-1 provides the esp-video DVP device (`/dev/video2`). Boards
/// without an onboard camera
/// return ESP_ERR_NOT_SUPPORTED.
esp_err_t livekit_s31_board_camera_init(void);

/// Apply the selected board's fixed camera-mount orientation.
esp_err_t livekit_s31_board_camera_apply_orientation(void);

/// Set the board's single RGB pixel to red, green, blue, or white.
bool livekit_s31_board_set_led(const char *color, bool state);

#ifdef __cplusplus
}
#endif
