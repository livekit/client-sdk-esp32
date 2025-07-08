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
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Peer error code
 */
typedef enum {
    ESP_PEER_ERR_NONE         = 0,  /*!< None error */
    ESP_PEER_ERR_INVALID_ARG  = -1, /*!< Invalid argument */
    ESP_PEER_ERR_NO_MEM       = -2, /*!< Not enough memory */
    ESP_PEER_ERR_WRONG_STATE  = -3, /*!< Operate on wrong state */
    ESP_PEER_ERR_NOT_SUPPORT  = -4, /*!< Not supported operation */
    ESP_PEER_ERR_NOT_EXISTS   = -5, /*!< Not existed */
    ESP_PEER_ERR_FAIL         = -6, /*!< General error code */
    ESP_PEER_ERR_OVER_LIMITED = -7, /*!< Overlimited */
    ESP_PEER_ERR_BAD_DATA     = -8, /*!< Bad input data */
} esp_peer_err_t;

/**
 * @brief  ICE server configuration
 */
typedef struct {
    char *stun_url; /*!< STUN/Relay server URL */
    char *user;     /*!< User name */
    char *psw;      /*!< User password */
} esp_peer_ice_server_cfg_t;

/**
 * @brief  Peer role
 */
typedef enum {
    ESP_PEER_ROLE_CONTROLLING, /*!< Controlling role who initialize the connection */
    ESP_PEER_ROLE_CONTROLLED,  /*!< Controlled role */
} esp_peer_role_t;

#ifdef __cplusplus
}
#endif
