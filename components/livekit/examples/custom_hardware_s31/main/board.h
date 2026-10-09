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

#include "esp_codec_dev.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize the selected ESP32-S31 board's I2C, I2S, codec, and amplifier.
void board_init(void);

/// Return the selected board's playback codec device.
esp_codec_dev_handle_t get_playback_handle(void);

/// Return the selected board's record codec device.
esp_codec_dev_handle_t get_record_handle(void);

/// Return the PCM channel count required by the selected speaker output.
unsigned get_playback_channels(void);

#ifdef __cplusplus
}
#endif
