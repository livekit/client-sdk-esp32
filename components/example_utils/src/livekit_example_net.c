/*
 * Copyright 2025 LiveKit, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include <nvs_flash.h>
#include <string.h>

#if CONFIG_LK_EXAMPLE_USE_WIFI
#include "esp_wifi.h"
#elif CONFIG_LK_EXAMPLE_USE_ETHERNET
#include "esp_eth.h"
#include "ethernet_init.h"
#endif

#include "livekit_example_net.h"

// MARK: - Constants

static const char *TAG = "network_connect";

#define NETWORK_EVENT_CONNECTED 1 << 0
#define NETWORK_EVENT_FAILED    1 << 1

// MARK: - State

typedef struct {
    EventGroupHandle_t event_group;
    int retry_attempt;
} network_connect_t;

static network_connect_t state = {};

// MARK: - Common

static void ip_event_handler(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data
) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Connected: ip=" IPSTR ", gateway=" IPSTR,
        IP2STR(&event->ip_info.ip), IP2STR(&event->ip_info.gw));

    state.retry_attempt = 0;
    xEventGroupSetBits(state.event_group, NETWORK_EVENT_CONNECTED);
}

static inline void init_common(void)
{
    if (!state.event_group) {
        state.event_group = xEventGroupCreate();
    }
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
}

static inline bool wait_for_connection_or_failure(void)
{
    EventBits_t bits;
    do {
        bits = xEventGroupWaitBits(
            state.event_group,
            NETWORK_EVENT_CONNECTED | NETWORK_EVENT_FAILED,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY
        );
        if (bits & NETWORK_EVENT_CONNECTED) {
            return true;
        }
    } while (!(bits & NETWORK_EVENT_FAILED));
    return false;
}


// MARK: - WiFi
#if CONFIG_LK_EXAMPLE_USE_WIFI

static void wifi_event_handler(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data
) {
    switch (event_id) {
    case WIFI_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        if (CONFIG_LK_EXAMPLE_NETWORK_MAX_RETRIES < 0 ||
            state.retry_attempt < CONFIG_LK_EXAMPLE_NETWORK_MAX_RETRIES) {
            ESP_LOGI(TAG, "Retry: attempt=%d", state.retry_attempt + 1);
            esp_wifi_connect();
            state.retry_attempt++;
            return;
        }
        ESP_LOGE(TAG, "Unable to establish connection");
        xEventGroupSetBits(state.event_group, NETWORK_EVENT_FAILED);
        break;
    default:
        break;
    }
}

static inline bool connect_wifi(void)
{
    if (strlen(CONFIG_LK_EXAMPLE_WIFI_SSID) == 0) {
        ESP_LOGE(TAG, "WiFi SSID is empty");
        return false;
    }
    if (strlen(CONFIG_LK_EXAMPLE_WIFI_PASSWORD) == 0) {
        // Ok in the case of an open network, just inform the user
        // in case this is unexpected.
        ESP_LOGI(TAG, "WiFi password is empty");
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));

    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler,
        NULL
    ));
    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &ip_event_handler,
        NULL
    ));

    wifi_config_t wifi_config = {};
    strlcpy((char *)wifi_config.sta.ssid, CONFIG_LK_EXAMPLE_WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, CONFIG_LK_EXAMPLE_WIFI_PASSWORD, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    ESP_LOGI(TAG, "Connecting WiFi: ssid=%s", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_start());
    return true;
}

// MARK: - Ethernet
#elif CONFIG_LK_EXAMPLE_USE_ETHERNET

static void eth_event_handler(
    void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data
) {
    uint8_t mac_addr[6] = { 0 };
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGD(TAG, "Ethernet Link Up");
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        if (CONFIG_LK_EXAMPLE_NETWORK_MAX_RETRIES < 0 ||
            state.retry_attempt < CONFIG_LK_EXAMPLE_NETWORK_MAX_RETRIES) {
            ESP_LOGI(TAG, "Retry: attempt=%d", state.retry_attempt + 1);
            state.retry_attempt++;
            return;
        }
        ESP_LOGE(TAG, "Unable to establish connection");
        xEventGroupSetBits(state.event_group, NETWORK_EVENT_FAILED);
        break;
    default:
        break;
    }
}

static inline bool connect_ethernet(void)
{
    static esp_eth_handle_t *handles = NULL;

    uint8_t port_count = 0;
    ESP_ERROR_CHECK(ethernet_init_all(&handles, &port_count));

    if (port_count == 1) {
        esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
        esp_netif_t *eth_netif = esp_netif_new(&cfg);
        ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(handles[0])));
    } else {
        esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
        esp_netif_config_t cfg_spi = {
            .base = &esp_netif_config,
            .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH
        };
        char if_key_str[10];
        char if_desc_str[10];
        char num_str[3];
        for (int i = 0; i < port_count; i++) {
            itoa(i, num_str, 10);
            strcat(strcpy(if_key_str, "ETH_"), num_str);
            strcat(strcpy(if_desc_str, "eth"), num_str);
            esp_netif_config.if_key = if_key_str;
            esp_netif_config.if_desc = if_desc_str;
            esp_netif_config.route_prio -= i * 5;
            esp_netif_t *eth_netif = esp_netif_new(&cfg_spi);
            ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(handles[i])));
        }
    }
    ESP_ERROR_CHECK(esp_event_handler_register(
        ETH_EVENT,
        ESP_EVENT_ANY_ID,
        &eth_event_handler,
        NULL
    ));
    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT,
        IP_EVENT_ETH_GOT_IP,
        &ip_event_handler,
        NULL
    ));
    for (int i = 0; i < port_count; i++) {
        ESP_ERROR_CHECK(esp_eth_start(handles[i]));
    }
    ESP_LOGI(TAG, "Connecting Ethernet");
    return true;
}
#endif

// MARK: - Public API

bool lk_example_network_connect()
{
    init_common();
    bool success = false;
#if CONFIG_LK_EXAMPLE_USE_WIFI
    success = connect_wifi();
#elif CONFIG_LK_EXAMPLE_USE_ETHERNET
    success = connect_ethernet();
#endif
    if (!success) {
        return false;
    }
    return wait_for_connection_or_failure();
}
