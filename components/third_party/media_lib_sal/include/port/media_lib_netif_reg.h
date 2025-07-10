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
#ifndef MEDIA_LIB_NETIF_REG_H
#define MEDIA_LIB_NETIF_REG_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t addr;  /*!< IPv4 address */
} media_lib_ipv4_addr_t;

typedef struct {
    media_lib_ipv4_addr_t ip;      /**< Interface IPV4 address */
    media_lib_ipv4_addr_t netmask; /**< Interface IPV4 netmask */
    media_lib_ipv4_addr_t gw;      /**< Interface IPV4 gateway address */
} media_lib_ipv4_info_t;

typedef enum {
    MEDIA_LIB_NET_TYPE_STA = 0,     /**< Wi-Fi STA (station) interface */
    MEDIA_LIB_NET_TYPE_AP,          /**< Wi-Fi soft-AP interface */
    MEDIA_LIB_NET_TYPE_ETH,         /**< Ethernet interface */
} media_lib_net_type_t;

typedef int (*__media_lib_netif_get_ipv4_info)(media_lib_net_type_t type, media_lib_ipv4_info_t *ip_info);
typedef char* (*__media_lib_ipv4_ntoa)(const media_lib_ipv4_addr_t *addr);
typedef struct {
    __media_lib_netif_get_ipv4_info get_ipv4_info;  /*!< Get ipv4 information */
    __media_lib_ipv4_ntoa           ipv4_ntoa;
} media_lib_netif_t;

/**
 * @brief     Register network interface related wrapper functions for media library
 *
 * @param      netif_lib: Network interface wrapper function lists
 *
* @return
*             - ESP_OK: On success
*             - ESP_ERR_INVALID_ARG: Some members of network interface lib not set
*/
esp_err_t media_lib_netif_register(media_lib_netif_t *netif_lib);

#ifdef __cplusplus
}
#endif

#endif
