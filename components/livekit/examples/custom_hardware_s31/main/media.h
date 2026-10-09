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

#include "esp_capture.h"
#include "av_render.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize the capture (microphone) and render (speaker) pipelines.
int media_init(void);

/// Get the capturer handle for publishing audio to a LiveKit room.
esp_capture_handle_t media_get_capturer(void);

/// Get the renderer handle for playing audio from a LiveKit room.
av_render_handle_t media_get_renderer(void);

#ifdef __cplusplus
}
#endif
