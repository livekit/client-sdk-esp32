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

#include <stdint.h>
#include "esp_peer_signaling.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  WHIP authorization type
 */
typedef enum {
    ESP_PEER_SIGNALING_WHIP_AUTH_TYPE_BEARER = 0, /*!< Use Bearer token */
    ESP_PEER_SIGNALING_WHIP_AUTH_TYPE_BASIC  = 1, /*!< Use basic authorization */
} esp_peer_signaling_whip_auth_type_t;

/**
 * @brief  WHIP signaling configuration
 */
typedef struct {
    esp_peer_signaling_whip_auth_type_t auth_type; /*!< Authorization type */
    char                               *token;     /*!< Bearer token or username:password for basic authorization */
} esp_peer_signaling_whip_cfg_t;

#ifdef __cplusplus
}
#endif
