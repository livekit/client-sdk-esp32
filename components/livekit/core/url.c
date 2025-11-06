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

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_idf_version.h"
#include "esp_chip_info.h"

#include "url.h"

static const char *TAG = "livekit_url";

#define URL_PARAM_SDK      "esp32"
#define URL_PARAM_VERSION  LIVEKIT_SDK_VERSION
#define URL_PARAM_OS       "idf"
#define URL_PARAM_PROTOCOL "1"

// TODO: For now, we use a protocol version that does not support subscriber primary.
// This is to get around a limitation with re-negotiation.

#define URL_FORMAT "%s%srtc?" \
    "sdk=" URL_PARAM_SDK \
    "&version=" URL_PARAM_VERSION \
    "&os=" URL_PARAM_OS \
    "&os_version=%s" \
    "&device_model=%d" \
    "&auto_subscribe=false" \
    "&protocol=" URL_PARAM_PROTOCOL

bool url_build(const url_build_options *options, char **out_url)
{
    if (out_url == NULL ||
        options == NULL ||
        options->server_url == NULL) {
        return false;
    }
    size_t server_url_len = strlen(options->server_url);
    if (server_url_len < 1) {
        ESP_LOGE(TAG, "Server URL cannot be empty");
        return false;
    }
    if (strncmp(options->server_url, "ws://", 5)  != 0 &&
        strncmp(options->server_url, "wss://", 6) != 0) {
        ESP_LOGE(TAG, "Unsupported URL scheme");
        return false;
    }
    // Do not add a trailing slash if the URL already has one
    const char *separator = options->server_url[server_url_len - 1] == '/' ? "" : "/";

    // Get chip and OS information
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    int model_code = chip_info.model;
    const char* idf_version = esp_get_idf_version();

    // TODO: Now that token is not included in the URL, use a fixed size buffer
    asprintf(out_url, URL_FORMAT,
        options->server_url,
        separator,
        idf_version,
        model_code
    );
    if (*out_url == NULL) {
        return false;
    }
    return true;
}