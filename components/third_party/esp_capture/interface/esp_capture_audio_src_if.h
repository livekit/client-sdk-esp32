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
 * @brief  Capture audio source interface
 */
typedef struct esp_capture_audio_src_if_t esp_capture_audio_src_if_t;

struct esp_capture_audio_src_if_t {
    /**
     * @brief  Open the audio source for capturing.
     */
    int (*open)(esp_capture_audio_src_if_t *src);

    /**
     * @brief  Get the supported audio codecs.
     */
    int (*get_support_codecs)(esp_capture_audio_src_if_t *src, const esp_capture_codec_type_t **codecs, uint8_t *num);

    /**
     * @brief  Negotiate capabilities between the source and the sink.
     */
    int (*negotiate_caps)(esp_capture_audio_src_if_t *src, esp_capture_audio_info_t *in_cap, esp_capture_audio_info_t *out_caps);

    /**
     * @brief  Start capturing audio from the source.
     */
    int (*start)(esp_capture_audio_src_if_t *src);

    /**
     * @brief  Read a frame of audio data from the source.
     */
    int (*read_frame)(esp_capture_audio_src_if_t *src, esp_capture_stream_frame_t *frame);

    /**
     * @brief  Stop capturing audio from the source.
     */
    int (*stop)(esp_capture_audio_src_if_t *src);

    /**
     * @brief  Close the audio source and release resources.
     */
    int (*close)(esp_capture_audio_src_if_t *src);
};

#ifdef __cplusplus
}
#endif