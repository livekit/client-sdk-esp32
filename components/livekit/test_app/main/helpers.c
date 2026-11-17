/*
 * Copyright 2026 LiveKit, Inc.
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
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "example_common_private.h" // example_wifi_start(), example_wifi_sta_do_connect()
#include "unity.h"

#include "helpers.h"

void read_param(const char *name, char *buf, size_t len)
{
    // Not printf: mixing it with unity_gets' echo drops bytes
    UnityPrint("Enter ");
    UnityPrint(name);
    UNITY_PRINT_EOL();
    UNITY_OUTPUT_FLUSH();
    unity_gets(buf, len);
}

void network_connect(const char *ssid, const char *password)
{
    TEST_ESP_OK(nvs_flash_init());
    TEST_ESP_OK(esp_netif_init());
    TEST_ESP_OK(esp_event_loop_create_default());
#if CONFIG_EXAMPLE_CONNECT_WIFI
    wifi_config_t config = {0};
    // Not strlcpy: a full-length SSID or password has no NUL terminator
    memcpy(config.sta.ssid, ssid, strnlen(ssid, sizeof(config.sta.ssid)));
    memcpy(config.sta.password, password, strnlen(password, sizeof(config.sta.password)));
    example_wifi_start();
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, example_wifi_sta_do_connect(config, true), "Wi-Fi connection failed");
#else
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, example_connect(), "Network connection failed");
#endif
}
