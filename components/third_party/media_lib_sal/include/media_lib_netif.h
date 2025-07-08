/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2023 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#ifndef MEDIA_LIB_NETIF_H
#define MEDIA_LIB_NETIF_H

#include "media_lib_netif_reg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief      Wrapper for get ipv4 information
 * @return     - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *             - Others: returned by wrapper function directly
 */
int media_lib_netif_get_ipv4_info(media_lib_net_type_t type, media_lib_ipv4_info_t *ip_info);

/**
 * @brief      Wrapper for convert ipv4 into string
 * @return     - NULL: wrapper function not registered
 *             - Others: returned by wrapper function directly
 */
char* media_lib_ipv4_ntoa(const media_lib_ipv4_addr_t *addr);

#ifdef __cplusplus
}
#endif

#endif