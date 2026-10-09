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

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize the Korvo-1 DVP camera as ESP_VIDEO_DVP_DEVICE_NAME.
esp_err_t s31_korvo_camera_init(void);

/// Rotate the onboard camera image to match the Korvo-1's physical mounting.
esp_err_t s31_korvo_camera_apply_orientation(void);

#ifdef __cplusplus
}
#endif
