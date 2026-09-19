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
