/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2021 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#ifndef MEDIA_LIB_ADAPTER_H
#define MEDIA_LIB_ADAPTER_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief      Add all external library that media used using default function wrapper
 *
 * @return
 *             - ESP_OK: on success
 *             - ESP_ERR_INVALID_ARG: wrapper functions not OK
 */
esp_err_t media_lib_add_default_adapter(void);

/**
 * @brief      Add default OS wrapper fuctions
 *
 * @return
 *             - ESP_OK: on success
 *             - ESP_ERR_INVALID_ARG: OS wrapper functions not OK
 */
esp_err_t media_lib_add_default_os_adapter(void);

/**
 * @brief      Add default crypt related wrapper functions
 *
 * @return
 *             - ESP_OK: on success
 *             - ESP_ERR_INVALID_ARG: crypt wrapper functions not OK
 */
esp_err_t media_lib_add_default_crypt_adapter(void);

/**
 * @brief      Add default socket related wrapper functions
 *
 * @return
 *             - ESP_OK: on success
 *             - ESP_ERR_INVALID_ARG: socket wrapper functions not OK
 */
esp_err_t media_lib_add_default_socket_adapter(void);

/**
 * @brief      Add default tls related wrapper functions
 *
 * @return
 *             - ESP_OK: on success
 *             - ESP_ERR_INVALID_ARG: tls wrapper functions not OK
 */
esp_err_t media_lib_add_default_tls_adapter(void);

/**
 * @brief      Add default network interface related wrapper functions
 *
 * @return
 *             - ESP_OK: on success
 *             - ESP_ERR_INVALID_ARG: Network interface wrapper functions not OK
 */
esp_err_t media_lib_add_default_netif_adapter(void);

#ifdef __cplusplus
}
#endif

#endif
