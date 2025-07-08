/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2022 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#ifndef MEDIA_LIB_TLS_H
#define MEDIA_LIB_TLS_H

#include "media_lib_tls_reg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief      Wrapper for create tls client instance
 * @return     - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *             - Others: returned by wrapper function directly
 */
media_lib_tls_handle_t media_lib_tls_new(const char *hostname, int hostlen, int port, const media_lib_tls_cfg_t *cfg);

/**
 * @brief      Wrapper for create tls server instance
 * @return     - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *             - Others: returned by wrapper function directly
 */
media_lib_tls_handle_t media_lib_tls_new_server(int fd, const media_lib_tls_server_cfg_t *cfg);

/**
 * @brief      Wrapper for esp_tls write
 * @return     - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *             - Others: returned by wrapper function directly
 */
int media_lib_tls_write(media_lib_tls_handle_t tls, const void *data, size_t datalen);

/**
 * @brief      Wrapper for esp_tls read
 * @return     - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *             - Others: returned by wrapper function directly
 */
int media_lib_tls_read(media_lib_tls_handle_t tls, void *data, size_t datalen);

/**
 * @brief      Wrapper for esp_tls getsockfd
 * @return     - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *             - Others: returned by wrapper function directly
 */
int media_lib_tls_getsockfd(media_lib_tls_handle_t tls);

/**
 * @brief      Wrapper for esp_tls delete
 * @return     - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 */
int media_lib_tls_delete(media_lib_tls_handle_t tls);

/**
 * @brief      Wrapper for esp_tls get bytes avail
 * @return     - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *             - Others: returned by wrapper function directly
 */
int media_lib_tls_get_bytes_avail(media_lib_tls_handle_t tls);

#ifdef __cplusplus
}
#endif

#endif
