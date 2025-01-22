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
 * @brief  Capture audio encoder interface
 */
typedef struct esp_capture_aenc_if_t esp_capture_aenc_if_t;

struct esp_capture_aenc_if_t {
    /**
     * @brief  Clone an existing audio encoder.
     */
    esp_capture_aenc_if_t *(*clone)(esp_capture_aenc_if_t *enc);

    /**
     * @brief  Get the supported audio codecs.
     */
    int (*get_support_codecs)(esp_capture_aenc_if_t *src, const esp_capture_codec_type_t **codecs, uint8_t *num);

    /**
     * @brief  Start the audio encoder.
     */
    int (*start)(esp_capture_aenc_if_t *src, esp_capture_audio_info_t *info);

    /**
     * @brief  Get the input and output frame sizes for the audio encoder.
     */
    int (*get_frame_size)(esp_capture_aenc_if_t *src, int *in_samples, int *out_frame_size);

    /**
     * @brief  Set the bitrate for the audio encoder.
     */
    int (*set_bitrate)(esp_capture_aenc_if_t *src, int bitrate);

    /**
     * @brief  Encode an audio frame.
     */
    int (*encode_frame)(esp_capture_aenc_if_t *src, esp_capture_stream_frame_t *raw, esp_capture_stream_frame_t *encoded);

    /**
     * @brief  Stop the audio encoder.
     */
    int (*stop)(esp_capture_aenc_if_t *src);
};

#ifdef __cplusplus
}
#endif