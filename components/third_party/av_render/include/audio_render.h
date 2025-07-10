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
 * @brief  Audio render handle
 */
typedef void *audio_render_handle_t;

/**
 * @brief  Initialize audio render callback
 *
 * @param[in]  cfg       Audio render configuration
 * @param[in]  cfg_size  Audio render configuration size
 *
 * @return
 *       - NULL    Not enough memory
 *       - Others  On success
 */
typedef audio_render_handle_t (*audio_render_int_func)(void *cfg, int cfg_size);

/**
 * @brief  Open audio render callback
 *
 * @param[in]  render  Audio render handle
 * @param[in]  info    Audio frame information
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to open audio render
 */
typedef int (*audio_render_open_func)(audio_render_handle_t render, av_render_audio_frame_info_t *info);

/**
 * @brief  Write to audio render callback
 *
 * @param[in]  render      Audio render handle
 * @param[in]  audio_data  Audio data to be written
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to write
 */
typedef int (*audio_render_write_func)(audio_render_handle_t render, av_render_audio_frame_t *audio_data);

/**
 * @brief  Get audio render latency callback
 *
 * @param[in]   render   Audio render handle
 * @param[out]  latency  Audio render latency
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to get latency
 */
typedef int (*audio_render_latency_func)(audio_render_handle_t render, uint32_t *latency);

/**
 * @brief  Get audio render frame information
 *
 * @param[in]   render  Audio render handle
 * @param[out]  info    Audio frame information
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to get
 */
typedef int (*audio_render_get_frame_info_func)(audio_render_handle_t render, av_render_audio_frame_info_t *info);

/**
 * @brief  Set audio render speed
 *
 * @param[in]  render  Audio render handle
 * @param[in]  speed   Audio render speed
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to set speed
 */
typedef int (*audio_render_set_speed_func)(audio_render_handle_t render, float speed);

/**
 * @brief  Close audio render callback
 *
 * @param[in]  render  Audio render handle
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to close
 */
typedef int (*audio_render_close_func)(audio_render_handle_t render);

/**
 * @brief  Deinitialize audio render callback
 *
 * @param[in]  render  Audio render handle
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to deinitialize
 */
typedef void (*audio_render_deinit_func)(audio_render_handle_t render);

/**
 * @brief  Audio render operations
 */
typedef struct {
    audio_render_int_func            init;           /*!< Initialize audio render callback */
    audio_render_open_func           open;           /*!< Open audio render callback */
    audio_render_write_func          write;          /*!< Write to audio render callback */
    audio_render_latency_func        get_latency;    /*!< Get audio render latency callback */
    audio_render_get_frame_info_func get_frame_info; /*!< Get audio frame information callback */
    audio_render_set_speed_func      set_speed;      /*!< Set audio render speed callback */
    audio_render_close_func          close;          /*!< Close audio render callback */
    audio_render_deinit_func         deinit;         /*!< Deinitialize audio render callback */
} audio_render_ops_t;

/**
 * @brief  Audio render configuration
 */
typedef struct {
    audio_render_ops_t ops;      /*!< Audio render implementation */
    void              *cfg;      /*!< Audio render configuration */
    int                cfg_size; /*!< Audio render configuration size */
} audio_render_cfg_t;

/**
 * @brief  Allocate audio render handle
 *
 * @param[in]  cfg  Audio render configuration
 *
 * @return
 *       - NULL    Not enough memory
 *       - Others  On success
 */
audio_render_handle_t audio_render_alloc_handle(audio_render_cfg_t *cfg);

/**
 * @brief  Open audio render
 *
 * @param[in]  render  Audio render handle
 * @param[in]  info    Audio frame information
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to open audio render
 */
int audio_render_open(audio_render_handle_t render, av_render_audio_frame_info_t *info);

/**
 * @brief  Write data to audio render
 *
 * @param[in]  render      Audio render handle
 * @param[in]  audio_data  Data to be written
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to write
 */
int audio_render_write(audio_render_handle_t render, av_render_audio_frame_t *audio_data);

/**
 * @brief  Get audio render latency
 *
 * @param[in]   render   Audio render handle
 * @param[out]  latency  Audio latency (unit ms)
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to get latency
 */
int audio_render_get_latency(audio_render_handle_t render, uint32_t *latency);

/**
 * @brief  Set audio render speed
 *
 * @note  Speed 1.0 means normal speed, 0.0 means pause
 *
 * @param[in]  render  Audio render handle
 * @param[in]  speed   Audio render speed
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to set speed
 */
int audio_render_set_speed(audio_render_handle_t render, float speed);

/**
 * @brief  Get audio frame information from audio render
 *
 * @param[in]   render  Audio render handle
 * @param[out]  info    Audio frame information
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to get
 */
int audio_render_get_frame_info(audio_render_handle_t render, av_render_audio_frame_info_t *info);

/**
 * @brief  Close audio render
 *
 * @param[in]  render  Audio render handle
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to close
 */
int audio_render_close(audio_render_handle_t render);

/**
 * @brief  Free audio render
 *
 * @param[in]  render  Audio render handle
 *
 */
void audio_render_free_handle(audio_render_handle_t render);

#ifdef __cplusplus
}
#endif
