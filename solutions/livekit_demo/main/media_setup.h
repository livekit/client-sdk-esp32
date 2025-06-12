
#pragma once

#include <esp_capture.h>
#include <av_render.h>

#ifdef __cplusplus
extern "C" {
#endif

int media_setup_init(void);
esp_capture_handle_t media_setup_get_capturer(void);
av_render_handle_t media_setup_get_renderer(void);

#ifdef __cplusplus
}
#endif

