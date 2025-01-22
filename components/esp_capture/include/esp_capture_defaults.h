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
#include "esp_capture_video_src_if.h"
#include "esp_capture_audio_src_if.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  V4L2 video source configuration
 */
typedef struct {
    uint8_t  buf_count;  /*!< Buffer count to buffer video frame */
    int16_t  pwr_pin;    /*!< Power pin */
    int16_t  reset_pin;  /*!< Reset pin */
    int16_t  xclk_pin;   /*!< XCLK pin */
    int16_t  data[8];    /*!< Data pins */
    int16_t  vsync_pin;  /*!< VSYNC pin */
    int16_t  href_pin;   /*!< HREF pin */
    int16_t  pclk_pin;   /*!< PCLK pin */
    uint32_t xclk_freq;  /*!< XCLK frequency */
    void    *i2c_master; /*!< I2C master handle*/
} esp_capture_video_dvp_src_cfg_t;

/**
 * @brief  Create an instance for DVP video source
 *
 * @param[in]  cfg  DVP video source configuration
 *
 * @return
 *       - NULL    Not enough memory to hold DVP instance
 *       - Others  DVP video source instance
 *
 */
esp_capture_video_src_if_t *esp_capture_new_video_dvp_src(esp_capture_video_dvp_src_cfg_t *cfg);

/**
 * @brief  V4L2 video source configuration
 */
typedef struct {
    char    dev_name[16]; /*!< Device name */
    uint8_t buf_count;    /*!< Frame buffer count */
} esp_capture_video_v4l2_src_cfg_t;

/**
 * @brief  Create an instance for V4L2 video source
 *
 * @param[in]  cfg  V4L2 video source configuration
 *
 * @return
 *       - NULL    Not enough memory to hold V4L2 instance
 *       - Others  V4L2 video source instance
 *
 */
esp_capture_video_src_if_t *esp_capture_new_video_v4l2_src(esp_capture_video_v4l2_src_cfg_t *cfg);

/**
 * @brief  Audio codec source configuration
 */
typedef struct {
    void *record_handle; /*!< Record handle of `esp_codec_dev` */
} esp_capture_audio_codec_src_cfg_t;

/**
 * @brief  Create audio source instance for codec
 *
 * @param[in]  cfg  Audio codec source configuration
 *
 * @return
 *       - NULL    Not enough memory to hold audio codec source instance
 *       - Others  Audio codec source instance
 *
 */
esp_capture_audio_src_if_t *esp_capture_new_audio_codec_src(esp_capture_audio_codec_src_cfg_t *cfg);

/**
 * @brief  Audio with AEC source configuration
 */
typedef struct {
    void   *record_handle; /*!< Record handle of `esp_codec_dev` */
    uint8_t channel;       /*!< Audio channel */
    uint8_t channel_mask;  /*!< Selected channel */
} esp_capture_audio_aec_src_cfg_t;

/**
 * @brief  Enable to dump AEC input and output data
 *
 * @param[in]  enable  Whether enable dump AEC of not
 *
 */
void esp_capture_enable_aec_src_dump(bool enable);

/**
 * @brief  Create audio source with AEC for codec
 *
 * @param[in]  cfg  Audio source with AEC configuration
 *
 * @return
 *       - NULL    Not enough memory to hold audio codec source instance
 *       - Others  Audio codec source instance
 *
 */
esp_capture_audio_src_if_t *esp_capture_new_audio_aec_src(esp_capture_audio_aec_src_cfg_t *cfg);

#ifdef __cplusplus
}
#endif
