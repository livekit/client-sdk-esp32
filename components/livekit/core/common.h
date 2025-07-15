
#pragma once

#include "esp_peer.h"
#include "esp_capture.h"
#include "av_render.h"

#ifdef __cplusplus
extern "C" {
#endif

/// State of an engine component or the engine itself.
typedef enum {
    CONNECTION_STATE_DISCONNECTED = 0, /*!< Disconnected */
    CONNECTION_STATE_CONNECTING   = 1, /*!< Establishing connection */
    CONNECTION_STATE_CONNECTED    = 2, /*!< Connected */
    CONNECTION_STATE_RECONNECTING = 3, /*!< Connection was previously established, but was lost */
    CONNECTION_STATE_FAILED       = 4  /*!< Connection failed */
} connection_state_t;

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
