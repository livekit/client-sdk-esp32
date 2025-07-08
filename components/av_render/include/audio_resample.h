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

#ifdef __cplusplus
extern "C" {
#endif

#include "av_render_types.h"

/**
 * @brief  Audio resample handle
 */
typedef void *audio_resample_handle_t;

/**
 * @brief  Audio resample frame callback
 */
typedef int (*audio_resample_frame_cb)(av_render_audio_frame_t *frame, void *ctx);

/**
 * @brief  Audio resample configuration
 */
typedef struct {
    av_render_audio_frame_info_t input_info;   /*!< Input frame information */
    av_render_audio_frame_info_t output_info;  /*!< Output frame information */
    audio_resample_frame_cb      resample_cb;  /*!< Resample output callback */
    void                        *ctx;          /*!< User context */
} audio_resample_cfg_t;

/**
 * @brief  Open audio resample
 *
 * @param[in]  cfg  Audio resample configuration
 *
 * @return
 *       - NULL    No memory for resample instance
 *       - Others  Audio resample instance
 */
audio_resample_handle_t audio_resample_open(audio_resample_cfg_t *cfg);

/**
 * @brief  Write data to audio resample
 *
 * @param[in]  h     Audio resample handle
 * @param[in]  data  Data to be written
 *
 * @return
 *       - ESP_MEDIA_ERR_OK      On success
 *       - ESP_MEDIA_ERR_NO_MEM  No enough memory
 */
int audio_resample_write(audio_resample_handle_t h, av_render_audio_frame_t *data);

/**
 * @brief  Close audio resample
 *
 * @param[in]  h     Audio resample handle
 */
void audio_resample_close(audio_resample_handle_t h);

#ifdef __cplusplus
}
#endif
