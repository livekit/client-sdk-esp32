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
#include "audio_render.h"
#include "video_render.h"
#include "esp_lcd_panel_ops.h"
#include "esp_codec_dev.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Audio render reference callback
 */
typedef int (*audio_render_ref_cb)(uint8_t *data, int size, void *ctx);

/**
 * @brief  I2S render configuration
 */
typedef struct {
    esp_codec_dev_handle_t play_handle;  /*!< Playback handle */
    audio_render_ref_cb    cb;           /*!< Reference output callback */
    bool                   fixed_clock;  /*!< Fixed clock mode */
    void                  *ctx;          /*!< User context */
} i2s_render_cfg_t;

/**
 * @brief  LCD render configuration
 */
typedef struct {
    esp_lcd_panel_handle_t lcd_handle;        /*!< LCD display handle */
    bool                   rgb_panel;         /*!< Whether RGB panel */
    bool                   dsi_panel;         /*!< Whether DSI panel */
    bool                   use_frame_buffer;  /*!< Use display frame buffer */
} lcd_render_cfg_t;

/**
 * @brief  Allocate I2S render
 *
 * @param[in]  cfg  I2S render configuration
 *
 * @return
 *       - NULL    No memory for I2S render instance
 *       - Others  Audio render instance
 */
audio_render_handle_t av_render_alloc_i2s_render(i2s_render_cfg_t *cfg);

/**
 * @brief  Allocate LCD render
 *
 * @param[in]  cfg  LCD render configuration
 *
 * @return
 *       - NULL    No memory for LCD render instance
 *       - Others  video render instance
 */
video_render_handle_t av_render_alloc_lcd_render(lcd_render_cfg_t *cfg);

#ifdef __cplusplus
}
#endif
