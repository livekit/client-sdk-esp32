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

#include <string.h>
#include "esp_log.h"
#include "media_lib_netif_reg.h"
#include "media_lib_adapter.h"
#include "media_lib_os.h"
#include "esp_idf_version.h"
#include "lwip/ip_addr.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
#include "esp_netif.h"
#else
#include "tcpip_adapter_types.h"
#endif

#ifdef CONFIG_MEDIA_PROTOCOL_LIB_ENABLE

static int _get_ipv4_info(media_lib_net_type_t type, media_lib_ipv4_info_t *ip_info)
{
    int ret;
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
    esp_netif_ip_info_t local_ip;
    const char* name;
    switch (type) {
        case MEDIA_LIB_NET_TYPE_STA:
            name = "WIFI_STA_DEF";
            break;
        case MEDIA_LIB_NET_TYPE_AP:
            name = "WIFI_AP_DEF";
            break;
        case MEDIA_LIB_NET_TYPE_ETH:
            name = "ETH_DEF";
            break;
        default:
            return ESP_ERR_NOT_SUPPORTED;
    }
    ret = esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey(name), &local_ip);
#else
    tcpip_adapter_ip_info_t local_ip;
    tcpip_adapter_if_t if_type;
    switch (type) {
        case MEDIA_LIB_NET_TYPE_STA:
            if_type = TCPIP_ADAPTER_IF_STA;
            break;
        case MEDIA_LIB_NET_TYPE_AP:
            if_type = TCPIP_ADAPTER_IF_AP;
            break;
        case MEDIA_LIB_NET_TYPE_ETH:
            if_type = TCPIP_ADAPTER_IF_ETH;
            break;
        default:
            return ESP_ERR_NOT_SUPPORTED;
    }
    ret = tcpip_adapter_get_ip_info(if_type, &local_ip);
#endif
    if (ret == ESP_OK) {
        ip_info->ip.addr = local_ip.ip.addr;
        ip_info->netmask.addr = local_ip.netmask.addr;
        ip_info->gw.addr = local_ip.gw.addr;
    }
    return ret;
}

static char* _ipv4_ntoa(const media_lib_ipv4_addr_t *addr)
{
    if (sizeof(media_lib_ipv4_addr_t) == sizeof(ip4_addr_t)) {
        return ip4addr_ntoa((const ip4_addr_t*)addr);
    }
    return NULL;
}

esp_err_t media_lib_add_default_netif_adapter(void)
{
    media_lib_netif_t netif_lib = {
        .get_ipv4_info = _get_ipv4_info,
        .ipv4_ntoa = _ipv4_ntoa,
    };
    return media_lib_netif_register(&netif_lib);
}

#endif
