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

#include "esp_codec_dev.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  I2S mode
 */
typedef enum {
    CODEC_I2S_MODE_STD = 0, /*!< STD mode */
    CODEC_I2S_MODE_TDM,     /*!< TDM mode */
    CODEC_I2S_MODE_NONE,    /*!< NONE means not support */
} codec_i2s_mode_t;

/**
 * @brief  Codec initialize configuration
 */
typedef struct {
    codec_i2s_mode_t in_mode;    /*!< I2S input mode */
    codec_i2s_mode_t out_mode;   /*!< I2S output mode */
    bool             in_use_tdm; /*!< Whether codec use TDM mode for input (codec use TDM, I2S can use STD mode) */
    bool             reuse_dev;  /*!< Use same handle for both input and output */
} codec_init_cfg_t;

/**
 * @brief  Initialize codec
 *
 * @param[in]  cfg  Codec initialize configuration
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to initialize
 */
int init_codec(codec_init_cfg_t *cfg);

/**
 * @brief  Get `esp_codec_dev` handle for playback
 *
 * @return
 *       - NULL    Fail to get playback handle
 *       - Others  Playback `esp_codec_dev` handle
 */
esp_codec_dev_handle_t get_playback_handle(void);

/**
 * @brief  Get `esp_codec_dev` handle for record
 *
 * @return
 *       - NULL    Fail to get record handle
 *       - Others  Record `esp_codec_dev` handle
 */
esp_codec_dev_handle_t get_record_handle(void);

/**
 * @brief  Get I2C master bus handle by port
 *
 * @param[in]  port  I2C bus port
 *
 * @return
 *       - NULL    Fail to get I2C bus handle
 *       - Others  I2C bus handle
 */
void *get_i2c_bus_handle(uint8_t port);

/**
 * @brief  Mount sdcard
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to mount
 */
int mount_sdcard(void);

/**
 * @brief  Unmount sdcard
 */
void unmount_sdcard(void);

/**
 * @brief  Deinitialize codec
 */
void deinit_codec(void);

/**
 * @brief  Initialized for LCD
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to init LCD
 */
int board_lcd_init(void);

/**
 * @brief  Get LCD handle
 *
 * @return
 *       - NULL    Fail to get LCD handle
 *       - Others  LCD handle
 */
void *board_get_lcd_handle(void);

#ifdef __cplusplus
}
#endif
