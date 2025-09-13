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

#include <string.h>
#include <nvs_flash.h>
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

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

// MARK: - Event handler

static void event_handler(void* arg,
                          esp_event_base_t event_base,
                          int32_t event_id,
                          void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (CONFIG_LK_EXAMPLE_NETWORK_MAX_RETRIES < 0 || state.retry_attempt < CONFIG_LK_EXAMPLE_NETWORK_MAX_RETRIES) {
            ESP_LOGI(TAG, "Retry: attempt=%d", state.retry_attempt + 1);
            esp_wifi_connect();
            state.retry_attempt++;
            return;
        }
        ESP_LOGE(TAG, "Unable to establish connection");
        xEventGroupSetBits(state.event_group, NETWORK_EVENT_FAILED);
    } else if (event_base == IP_EVENT &&
                 event_id == IP_EVENT_STA_GOT_IP) {

        ip_event_got_ip_t* event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Connected: ip=" IPSTR ", gateway=" IPSTR,
            IP2STR(&event->ip_info.ip), IP2STR(&event->ip_info.gw));

        state.retry_attempt = 0;
        xEventGroupSetBits(state.event_group, NETWORK_EVENT_CONNECTED);
    }
}

// MARK: - Initialization & connection

static inline void init_common(void)
{
    if (!state.event_group) {
        state.event_group = xEventGroupCreate();
    }

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &event_handler,
        NULL,
        &instance_got_ip
    ));
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

    esp_event_handler_instance_t instance_any_id;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &event_handler,
        NULL,
        &instance_any_id
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

// MARK: - Public API

bool lk_example_network_connect()
{
    init_common();
    if (!connect_wifi()) {
        return false;
    }
    return wait_for_connection_or_failure();
}
