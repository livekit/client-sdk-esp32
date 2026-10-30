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

#include "driver/i2c_master.h"
#include "esp_codec_dev.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize the audio-only portion of the ESP32-S31-Korvo-1 BSP.
esp_err_t s31_korvo_audio_init(void);

/// Return the ES8389 DAC device used for speaker rendering.
esp_codec_dev_handle_t s31_korvo_audio_get_playback_handle(void);

/// Return the ES8389 ADC device used for microphone capture.
esp_codec_dev_handle_t s31_korvo_audio_get_record_handle(void);

/// Return the shared codec/camera I2C bus after audio initialization.
i2c_master_bus_handle_t s31_korvo_audio_get_i2c_bus_handle(void);

#ifdef __cplusplus
}
#endif
