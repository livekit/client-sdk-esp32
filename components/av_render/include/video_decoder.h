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
 * @brief  Video decoder frame callback
 */
typedef int (*vdec_frame_cb)(av_render_video_frame_t *frame, void *ctx);

/**
 * @brief  Video decoder configuration
 */
typedef struct {
    av_render_video_info_t       video_info; /*!< Video basic information */
    av_render_video_frame_type_t out_type;   /*!< Output frame type */
    vdec_frame_cb                frame_cb;   /*!< Video decoded frame callback */
    void                        *ctx;        /*!< Decoder context */
} vdec_cfg_t;

/**
 * @brief  Video decoder frame buffer callback configuration
 */
typedef struct {
    uint8_t *(*fb_fetch)(int align, int size, void *ctx);  /*!< Fetch frame buffer */
    int (*fb_return)(uint8_t *addr, bool drop, void *ctx); /*!< Return frame buffer */
    void *ctx;                                             /*!< Context */
} vdec_fb_cb_cfg_t;

/**
 * @brief  Video decoder handle
 */
typedef void *vdec_handle_t;

/**
 * @brief  Get video decoder output formats according decode type
 *
 * @param[in]      codec  Decode type
 * @param[out]     fmts   Output formats to be filled
 * @param[in,out]  fmts   Output formats number
 * @return
 *       - NULL    No resource for decoder
 *       - Others  Video decoder handle
 */
int vdec_get_output_formats(av_render_video_codec_t codec, av_render_video_frame_type_t* fmts, uint8_t* num);

/**
 * @brief  Open video decoder
 *
 * @param[in]  cfg  Video decoder configuration
 *
 * @return
 *       - NULL    No resource for decoder
 *       - Others  Video decoder handle
 */
vdec_handle_t vdec_open(vdec_cfg_t *cfg);

/**
 * @brief  Set frame buffer callback
 *
 * @param[in]  h    Video decoder handle
 * @param[in]  cfg  Frame buffer callback configuration
 *
 * @return
 *       - ESP_MEDIA_ERR_OK           On success
 *       - ESP_MEDIA_ERR_INVALID_ARG  Invalid argument
 */
int vdec_set_fb_cb(vdec_handle_t h, vdec_fb_cb_cfg_t *cfg);

/**
 * @brief  Do video decode
 *
 * @param[in]  h     Video decoder handle
 * @param[in]  data  Video data to be decoded
 *
 * @return
 *       - ESP_MEDIA_ERR_OK           On success
 *       - ESP_MEDIA_ERR_INVALID_ARG  Invalid argument
 *       - Other                      Fail to decode
 */
int vdec_decode(vdec_handle_t h, av_render_video_data_t *data);

/**
 * @brief  Set frame buffer
 *
 * @param[in]  h       Video decoder handle
 * @param[in]  buffer  Frame buffer to put decoded data
 *
 * @return
 *       - ESP_MEDIA_ERR_OK           On success
 *       - ESP_MEDIA_ERR_INVALID_ARG  Invalid argument
 *       - Other                      Fail to decode
 */
int vdec_set_frame_buffer(vdec_handle_t h, av_render_frame_buffer_t *buffer);

/**
 * @brief  Get frame information from video decoder
 *
 * @param[in]   h           Video decoder handle
 * @param[out]  frame_info  Video frame information
 *
 * @return
 *       - ESP_MEDIA_ERR_OK           On success
 *       - ESP_MEDIA_ERR_INVALID_ARG  Invalid argument
 *       - Other                      Fail to decode
 */
int vdec_get_frame_info(vdec_handle_t h, av_render_video_frame_info_t *frame_info);

/**
 * @brief  Close video decoder
 *
 * @param[in]  h  Video decoder handle
 *
 * @return
 *       - ESP_MEDIA_ERR_OK           On success
 *       - ESP_MEDIA_ERR_INVALID_ARG  Invalid argument
 */
int vdec_close(vdec_handle_t h);

#ifdef __cplusplus
}
#endif
