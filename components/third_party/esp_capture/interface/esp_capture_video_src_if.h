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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Video source interface definition
 */
typedef struct esp_capture_video_src_if_t esp_capture_video_src_if_t;

/**
 * @brief  Video source interface
 */
struct esp_capture_video_src_if_t {
    /**
     * @brief  Open video capture soruce
     */
    int (*open)(esp_capture_video_src_if_t *src);

    /**
     * @brief  Get supported codecs from video source
     */
    int (*get_support_codecs)(esp_capture_video_src_if_t *src, const esp_capture_codec_type_t **codecs, uint8_t *num);

    /**
     * @brief  Negotiate for video source capability
     */
    int (*negotiate_caps)(esp_capture_video_src_if_t *src, esp_capture_video_info_t *in_cap, esp_capture_video_info_t *out_caps);

    /**
     * @brief  Start of video source
     */
    int (*start)(esp_capture_video_src_if_t *src);

    /**
     * @brief  Acquire video frame from video source
     */
    int (*acquire_frame)(esp_capture_video_src_if_t *src, esp_capture_stream_frame_t *frame);

     /**
     * @brief  Release video frame from video source
     */
    int (*release_frame)(esp_capture_video_src_if_t *src, esp_capture_stream_frame_t *frame);

    /**
     * @brief  Stop of video source
     */
    int (*stop)(esp_capture_video_src_if_t *src);

    /**
     * @brief  Close of video source
     */
    int (*close)(esp_capture_video_src_if_t *src);
};

#ifdef __cplusplus
}
#endif