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

#include "esp_capture_audio_src_if.h"
#include "esp_codec_dev.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LIVEKIT_S31_AEC_ES8311, ///< Two words: microphone, DAC reference.
    LIVEKIT_S31_AEC_ES8389, ///< Four DMA words: DAC L, mic L, DAC R, mic R.
} livekit_s31_aec_codec_t;

/// Create a mono 16 kHz AEC source from native 48 kHz capture.
/// ES8389 uses 64-bit bus frames to include the appended DAC references;
/// its left mic/reference pair is selected. Feed identical speaker channels
/// for mono voice playback; independent stereo echo cancellation is not provided.
/// Owns processing state, not the codec handle. Call stop/close before freeing
/// the returned interface. Capture lifecycle calls must be serialized.
esp_capture_audio_src_if_t *livekit_s31_aec_source_new(esp_codec_dev_handle_t record,
                                                    livekit_s31_aec_codec_t codec);

#ifdef __cplusplus
}
#endif
