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

#include "esp_capture_path_if.h"
#include "esp_capture_aenc_if.h"
#include "esp_capture_venc_if.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Simple capture path configuration
 *
 * @note  Simple capture path consisted by one audio encoder and one video encoder
 *        It also support bypass mode, during bypass encoder not work,
 *        audio source data and video source data is sent to user directly.
 *
 */
typedef struct {
    esp_capture_aenc_if_t *aenc;             /*!< Audio encoder instance */
    esp_capture_venc_if_t *venc;             /*!< Video encoder instance */
    uint32_t               aenc_frame_count; /*!< Audio encoder output frame count */
    uint32_t               venc_frame_count; /*!< Video encoder output frame count */
} esp_capture_simple_path_cfg_t;

/**
 * @brief  Create simple capture path instance
 *
 * @param[in]  cfg  Audio codec source configuration
 *
 * @return
 *       - NULL    Not enough memory to hold simple capture path instance
 *       - Others  Simple capture path instance
 *
 */
esp_capture_path_if_t *esp_capture_build_simple_path(esp_capture_simple_path_cfg_t *cfg);

#ifdef __cplusplus
}
#endif
