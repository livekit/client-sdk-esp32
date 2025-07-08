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

#include "esp_capture_overlay_if.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Color definition in RGB565 format
 *
 */
#define COLOR_RGB565_BLACK          (0x0000)
#define COLOR_RGB565_NAVY           (0x000F)
#define COLOR_RGB565_DARKGREEN      (0x03E0)
#define COLOR_RGB565_DARKCYAN       (0x03EF)
#define COLOR_RGB565_MAROON         (0x7800)
#define COLOR_RGB565_PURPLE         (0x780F)
#define COLOR_RGB565_OLIVE          (0x7BE0)
#define COLOR_RGB565_LIGHTGREY      (0xC618)
#define COLOR_RGB565_DARKGREY       (0x7BEF)
#define COLOR_RGB565_BLUE           (0x001F)
#define COLOR_RGB565_GREEN          (0x07E0)
#define COLOR_RGB565_CYAN           (0x07FF)
#define COLOR_RGB565_RED            (0xF800)
#define COLOR_RGB565_MAGENTA        (0xF81F)
#define COLOR_RGB565_YELLOW         (0xFFE0)
#define COLOR_RGB565_WHITE          (0xFFFF)
#define COLOR_RGB565_ORANGE         (0xFD20)
#define COLOR_RGB565_GREENYELLOW    (0xAFE5)
#define COLOR_RGB565_PINK           (0xF81F)
#define COLOR_RGB565_SILVER         (0xC618)
#define COLOR_RGB565_GRAY           (0x8410)
#define COLOR_RGB565_LIME           (0x07E0)
#define COLOR_RGB565_TEAL           (0x0410)
#define COLOR_RGB565_FUCHSIA        (0xF81F)
#define COLOR_RGB565_ESP_BKGD       (0xD185)
#define ESP_PAINGER_FORMAT_SIZE_MAX (128)

/**
 * @brief  Capture text overlay draw information
 */
typedef struct {
    uint16_t color;     /*!< Draw color */
    uint16_t font_size; /*!< Draw font size, font for this size must be loaded use `menuconfig` */
    uint16_t x;         /*!< Draw x position inside of the text overlay */
    uint16_t y;         /*!< Draw y position inside of the text overlay */
} esp_capture_text_overlay_draw_info_t;

/**
 * @brief  Create text overlay instance
 *
 * @note  The text overlay must be smaller than the main capture frame size.
 *        The region's (x, y) position represents the top-left corner of the text overlay
 *        relative to the capture frame.
 *        The region's width and height define the size of the text overlay, where the
 *        text is drawn within this region.
 *
 * @param[in]  rgn  Text overlay region setting
 *
 * @return
 *       - NULL    Not enough memory to hold text overlay instance
 *       - Others  Text overlay instance
 *
 */
esp_capture_overlay_if_t *esp_capture_new_text_overlay(esp_capture_rgn_t *rgn);

/**
 * @brief  Draw text start
 *
 * @note  Drawing should occur between `esp_capture_text_overlay_draw_start` and `esp_capture_text_overlay_draw_finished`,
 *        to ensure the text overlay frame data is fully captured.
 *        Multiple draw actions can be performed between these two functions
 *
 * @param[in]  h  Text overlay instance
 *
 * @return
 *       - ESP_CAPTURE_ERR_NOT_SUPPORTED  Not supported action for not open yet
 *       - ESP_CAPTURE_ERR_OK             Draw start success
 *
 */
int esp_capture_text_overlay_draw_start(esp_capture_overlay_if_t *h);

/**
 * @brief  Draw text on text overlay
 *
 * @note  Supports drawing multiple lines of text separated by '\n'.
 *        Each subsequent line will be aligned to the x position of the first line.
 *
 * @param[in]  h     Text overlay instance
 * @param[in]  info  Drawing settings
 * @param[in]  str   String to be drawn
 *
 * @return
 *       - ESP_CAPTURE_ERR_NOT_SUPPORTED  Not supported action for not open yet or will overflow
 *       - ESP_CAPTURE_ERR_OK             Draw text success
 *
 */
int esp_capture_text_overlay_draw_text(esp_capture_overlay_if_t *h, esp_capture_text_overlay_draw_info_t *info, char *str);

/**
 * @brief  Draw text with a format similar to `sprintf` on the text overlay.
 *
 * @note  Behavior is the same as `esp_capture_text_overlay_draw_text`.
 *
 * @param[in]  h          Text overlay instance
 * @param[in]  info       Drawing settings
 * @param[in]  fmt        String format
 * @param[in]  Arguments  to be formatted
 *
 * @return
 *       - ESP_CAPTURE_ERR_NOT_SUPPORTED  Not supported action, either not open yet or will overflow
 *       - ESP_CAPTURE_ERR_OK             Drawn with format success
 *
 */
int esp_capture_text_overlay_draw_text_fmt(esp_capture_overlay_if_t *h, esp_capture_text_overlay_draw_info_t *info, const char *fmt, ...);

/**
 * @brief  Indicate draw text finished
 *
 * @param[in]  h  Text overlay instance
 *
 * @return
 *       - ESP_CAPTURE_ERR_NOT_SUPPORTED  Not supported action for not open yet
 *       - ESP_CAPTURE_ERR_OK             Draw finished success
 *
 */
int esp_capture_text_overlay_draw_finished(esp_capture_overlay_if_t *h);

/**
 * @brief  Clear a region on the text overlay
 *
 * @param[in]  h      Text overlay instance
 * @param[in]  rgn    Region to be cleared
 * @param[in]  color  Color used to clear the region
 *
 * @return
 *       - ESP_CAPTURE_ERR_NOT_SUPPORTED  Action not supported for not open yet
 *       - ESP_CAPTURE_ERR_INVALID_ARG    Region exceeds text overlay boundaries
 *       - ESP_CAPTURE_ERR_OK             Clear region success
 */
int esp_capture_text_overlay_clear(esp_capture_overlay_if_t *h, esp_capture_rgn_t *rgn, uint16_t color);

#ifdef __cplusplus
}
#endif