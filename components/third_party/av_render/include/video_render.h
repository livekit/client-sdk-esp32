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
 * @brief  Video render open callback
 */
typedef video_render_handle_t (*video_render_open_func)(void *cfg, int size);

/**
 * @brief  Video render check format supported callback
 */
typedef bool (*video_render_format_supported_func)(video_render_handle_t render, av_render_video_frame_type_t type);

/**
 * @brief  Set frame information for video render callback
 */
typedef int (*video_render_set_frame_info_func)(video_render_handle_t render, av_render_video_frame_info_t *info);

/**
 * @brief  Write data video render callback
 */
typedef int (*video_render_write_func)(video_render_handle_t render, av_render_video_frame_t *video_data);

/**
 * @brief  Get video render latency callback
 */
typedef int (*video_render_latency_func)(video_render_handle_t render, uint32_t *latency);

/**
 * @brief  Get frame buffer from video render callback
 */
typedef int (*video_render_get_frame_buffer_func)(video_render_handle_t render, av_render_frame_buffer_t *frame_buffer);

/**
 * @brief  Get frame information from video render callback
 */
typedef int (*video_render_get_frame_info_func)(video_render_handle_t render, av_render_video_frame_info_t *info);

/**
 * @brief  Clear of video render callback
 */
typedef int (*video_render_clear_func)(video_render_handle_t render);

/**
 * @brief  Close of video render callback
 */
typedef int (*video_render_close_func)(video_render_handle_t render);

/**
 * @brief  Video render operations
 */
typedef struct _render_video_ops {
    video_render_open_func             open;             /*!< Video render open */
    video_render_format_supported_func format_support;   /*!< Video render check format supported */
    video_render_set_frame_info_func   set_frame_info;   /*!< Set frame information */
    video_render_get_frame_buffer_func get_frame_buffer; /*!< Get frame buffer */
    video_render_write_func            write;            /*!< Write data to video render */
    video_render_latency_func          get_latency;      /*!< Get video render latency */
    video_render_get_frame_info_func   get_frame_info;   /*!< Get frame information */
    video_render_clear_func            clear;            /*!< Clear of video render */
    video_render_close_func            close;            /*!< Close of video render */
} video_render_ops_t;

/**
 * @brief  Video render configuration
 */
typedef struct {
    video_render_ops_t ops;       /*!< Video render implementation */
    void              *cfg;       /*!< Specified render configuration */
    int                cfg_size;  /*!< Specified render configuration size */
} video_render_cfg_t;

/**
 * @brief  Allocate video render
 *
 * @param[in]  cfg  Video render configuration
 *
 * @return
 *       - NULL    Not enough memory or invalid argument
 *       - Others  Video render instance
 */
video_render_handle_t video_render_alloc_handle(video_render_cfg_t *cfg);

/**
 * @brief  Check whether video format supported
 *
 * @param[in]  render      Video render handle
 * @param[in]  frame_type  Video frame type to be checked
 *
 * @return
 *       - true   Format is supported
 *       - false  Format not supported
 */
bool video_render_format_supported(video_render_handle_t render, av_render_video_frame_type_t frame_type);

/**
 * @brief  Open video render
 *
 * @param[in]  render  Video render handle
 * @param[in]  info    Video frame information
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to open
 */
int video_render_open(video_render_handle_t render, av_render_video_frame_info_t *info);

/**
 * @brief  Write frame data to video render
 *
 * @param[in]  render      Video render handle
 * @param[in]  video_data  Frame data to wrote
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to write
 */
int video_render_write(video_render_handle_t render, av_render_video_frame_t *video_data);

/**
 * @brief  Get latency of video render
 *
 * @param[in]   render   Video render handle
 * @param[out]  latency  Latency to get
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to get
 */
int video_render_get_latency(video_render_handle_t render, uint32_t *latency);

/**
 * @brief  Get frame buffer from video render
 *
 * @param[in]   render  Video render handle
 * @param[out]  buffer  Frame buffer
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to get
 */
int video_render_get_frame_buffer(video_render_handle_t render, av_render_frame_buffer_t *buffer);

/**
 * @brief  Get frame information from video render
 *
 * @param[in]   render  Video render handle
 * @param[out]  info    Video frame information
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to get
 */
int video_render_get_frame_info(video_render_handle_t render, av_render_video_frame_info_t *info);

/**
 * @brief  Close of video render
 *
 * @param[in]   render  Video render handle
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to close
 */
int video_render_close(video_render_handle_t render);

/**
 * @brief  Free of video render
 *
 * @param[in]   render  Video render handle
 */
void video_render_free_handle(video_render_handle_t render);

#ifdef __cplusplus
}
#endif
