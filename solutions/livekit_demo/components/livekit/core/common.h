
#pragma once

#include "esp_peer.h"
#include "esp_capture.h"
#include "av_render.h"

#ifdef __cplusplus
extern "C" {
#endif

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
