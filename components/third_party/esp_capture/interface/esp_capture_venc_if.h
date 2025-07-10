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
 * @brief  Capture video encoder interface definition
 */
typedef struct esp_capture_venc_if_t esp_capture_venc_if_t;

/**
 * @brief  Capture video encoder interface
 */
struct esp_capture_venc_if_t {
    /**
     * @brief  Clone of video encoder instance
     */
    esp_capture_venc_if_t *(*clone)(esp_capture_venc_if_t *enc);

    /**
     * @brief  Get supported codecs from video encoder
     */
    int (*get_support_codecs)(esp_capture_venc_if_t *enc, const esp_capture_codec_type_t **codecs, uint8_t *num);

    /**
     * @brief  Get input codecs of special output codec from video encoder
     */
    int (*get_input_codecs)(esp_capture_venc_if_t *enc, esp_capture_codec_type_t out_codec,
                            const esp_capture_codec_type_t **codecs, uint8_t *num);

    /**
     * @brief  Start video encoder
     */
    int (*start)(esp_capture_venc_if_t *enc, esp_capture_codec_type_t src_codec, esp_capture_video_info_t *info);

    /**
     * @brief  Get frame size from video encoder
     */
    int (*get_frame_size)(esp_capture_venc_if_t *enc, int *in_frame_size, int *out_frame_size);

    /**
     * @brief  Set bitrate for video encoder
     */
    int (*set_bitrate)(esp_capture_venc_if_t *enc, int bitrate);

    /**
     * @brief  Encode video frame
     */
    int (*encode_frame)(esp_capture_venc_if_t *enc, esp_capture_stream_frame_t *raw, esp_capture_stream_frame_t *encoded);

    /**
     * @brief  Stop of video encode
     */
    int (*stop)(esp_capture_venc_if_t *enc);
};

#ifdef __cplusplus
}
#endif