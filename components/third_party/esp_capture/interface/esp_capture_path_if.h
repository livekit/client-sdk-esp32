/**
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2025 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#pragma once

#include "esp_capture_types.h"
#include "esp_capture_overlay_if.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Capture path interface alias
 */
typedef struct esp_capture_path_if_t esp_capture_path_if_t;

/**
 * @brief  Capture path type
 */
typedef enum {
    ESP_CAPTURE_PATH_PRIMARY = 0, /*!< Primary capture path */
    ESP_CAPTURE_PATH_SECONDARY,   /*!< Secondary capture path */
    ESP_CAPTURE_PATH_THIRD,       /*!< Third capture path */
    ESP_CAPTURE_PATH_MAX,         /*!< Maximum of capture path supported */
} esp_capture_path_type_t;

/**
 * @brief  Capture path set type
 */
typedef enum {
    ESP_CAPTURE_PATH_SET_TYPE_NONE,          /*!< Set type NONE */
    ESP_CAPTURE_PATH_SET_TYPE_AUDIO_BITRATE, /*!< Set for audio bitrate */
    ESP_CAPTURE_PATH_SET_TYPE_VIDEO_BITRATE, /*!< Set for video bitrate */
    ESP_CAPTURE_PATH_SET_TYPE_VIDEO_FPS,     /*!< Set for video frame per second */
} esp_capture_path_set_type_t;

/**
 * @brief  Capture path event
 */
typedef enum {
    ESP_CAPTURE_PATH_EVENT_TYPE_NONE         = 0,
    ESP_CAPTURE_PATH_EVENT_AUDIO_STARTED     = 1,
    ESP_CAPTURE_PATH_EVENT_AUDIO_NOT_SUPPORT = 2,
    ESP_CAPTURE_PATH_EVENT_AUDIO_ERROR       = 3,
    ESP_CAPTURE_PATH_EVENT_VIDEO_STARTED     = 4,
    ESP_CAPTURE_PATH_EVENT_VIDEO_NOT_SUPPORT = 5,
    ESP_CAPTURE_PATH_EVENT_VIDEO_ERROR       = 6,
} esp_capture_path_event_type_t;

/**
 * @brief  Capture path configuration
 */
typedef struct {
    /**
     * @brief  Acquire a source frame for processing.
     */
    int (*acquire_src_frame)(void *src, esp_capture_stream_frame_t *frame, bool no_wait);

    /**
     * @brief  Release a previously acquired source frame.
     */
    int (*release_src_frame)(void *src, esp_capture_stream_frame_t *frame);

    /**
     * @brief  Negotiate video capabilities between source and destination.
    */
    int (*nego_video)(void *src, esp_capture_video_info_t *in_cap, esp_capture_video_info_t *out_caps);

    /**
     * @brief  Negotiate audio capabilities between source and destination.
     */
    int (*nego_audio)(void *src, esp_capture_audio_info_t *in_cap, esp_capture_audio_info_t *out_caps);

    /**
     * @brief  Notify that a frame has been processed.
     */
    int (*frame_processed)(void *src, esp_capture_path_type_t path, esp_capture_stream_frame_t *frame);

    /**
     * @brief  Notify path event to capture
     */
    int (*event_cb)(void *src, esp_capture_path_type_t path, esp_capture_path_event_type_t event);

    /**
     * @brief  Pointer to the source context.
     */
    void *src_ctx;
} esp_capture_path_cfg_t;

/**
 * @brief  Capture path interface
 */
struct esp_capture_path_if_t {
    /**
     * @brief  Open a capture path interface with specified configuration.
     */
    int (*open)(esp_capture_path_if_t *p, esp_capture_path_cfg_t *cfg);

    /**
     * @brief  Add a new path to the capture path interface.
     */
    int (*add_path)(esp_capture_path_if_t *p, esp_capture_path_type_t path, esp_capture_sink_cfg_t *sink);

    /**
     * @brief  Add an overlay to a specific path.
     */
    int (*add_overlay)(esp_capture_path_if_t *p, esp_capture_path_type_t path, esp_capture_overlay_if_t *overlay);

    /**
     * @brief  Enable or disable an overlay on a specific path.
     */
    int (*enable_overlay)(esp_capture_path_if_t *p, esp_capture_path_type_t path, bool enable);

    /**
     * @brief  Enable or disable a specific path.
     */
    int (*enable_path)(esp_capture_path_if_t *p, esp_capture_path_type_t path, bool enable);

    /**
     * @brief  Start the capture path interface.
     */
    int (*start)(esp_capture_path_if_t *p);

    /**
     * @brief  Retrieve the number of audio frame samples for a specific path.
     */
    int (*get_audio_frame_samples)(esp_capture_path_if_t *p, esp_capture_path_type_t path);

    /**
     * @brief  Configure a specific path with given settings.
     */
    int (*set)(esp_capture_path_if_t *p, esp_capture_path_type_t path, esp_capture_path_set_type_t type, void *cfg, int cfg_size);

    /**
     * @brief  Return a frame back to the capture path interface.
     */
    int (*return_frame)(esp_capture_path_if_t *p, esp_capture_path_type_t path, esp_capture_stream_frame_t *frame);

    /**
     * @brief  Stop the capture path interface.
     */
    int (*stop)(esp_capture_path_if_t *p);

    /**
     * @brief  Close the capture path interface.
     */
    int (*close)(esp_capture_path_if_t *p);
};

#ifdef __cplusplus
}
#endif
