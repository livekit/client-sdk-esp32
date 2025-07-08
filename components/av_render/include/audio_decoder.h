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

#include "av_render_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Audio decoder frame callback
 *
 * @param[in]  frame  Decoded audio frame
 * @param[in]  ctx    Decoder context set by user
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to decode
 */
typedef int (*adec_frame_cb)(av_render_audio_frame_t *frame, void *ctx);

/**
 * @brief  Audio decoder configuration
 */
typedef struct {
    av_render_audio_info_t audio_info; /*!< Audio information */
    adec_frame_cb          frame_cb;   /*!< Decoded frame callback */
    void                  *ctx;        /*!< Decoder context */
} adec_cfg_t;

/**
 * @brief  Audio decoder handle
 */
typedef void *adec_handle_t;

/**
 * @brief  Open audio decoder
 *
 * @param[in]  cfg  Audio decoder configuration
 *
 * @return
 *       - NULL    No memory for decoder instance
 *       - Others  On success
 */
adec_handle_t adec_open(adec_cfg_t *cfg);

/**
 * @brief  Decode audio
 *
 * @param[in]  h     Audio decoder handle
 * @param[in]  data  Audio data to be decoded
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to decode
 */
int adec_decode(adec_handle_t h, av_render_audio_data_t *data);

/**
 * @brief  Get decoded audio frame information
 *
 * @param[in]   h           Audio decoder handle
 * @param[out]  frame_info  Audio frame information
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to decode
 */
int adec_get_frame_info(adec_handle_t h, av_render_audio_frame_info_t *frame_info);

/**
 * @brief  Close audio decoder
 *
 * @param[in]  h  Audio decoder handle
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to decode
 */
int adec_close(adec_handle_t h);

#ifdef __cplusplus
}
#endif
