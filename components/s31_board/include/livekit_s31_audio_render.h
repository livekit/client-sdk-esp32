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

#include "audio_render.h"
#include "esp_codec_dev.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Allocate a buffered renderer for the S31 examples' shared I2S bus.
///
/// Configure capture and av_render's fixed output to 48000 Hz, 16-bit stereo.
/// The transport may use another rate/channel count; the media pipelines
/// perform conversion. Stereo I2S slots do not imply two physical speakers.
/// Volume is 0..100 and is reapplied after every codec open.
///
/// The owner must serialize open/close/free, as required by audio_render.
/// Close cancels queued PCM and waits for the writer to finish its current
/// codec call before closing the device. It may overlap a write callback.
/// The codec handle remains owned by the caller. Free using audio_render APIs.
audio_render_handle_t livekit_s31_audio_render_alloc(
    esp_codec_dev_handle_t playback_handle, int volume);

#ifdef __cplusplus
}
#endif
