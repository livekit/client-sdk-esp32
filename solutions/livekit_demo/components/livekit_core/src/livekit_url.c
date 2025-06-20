#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_idf_version.h"
#include "esp_chip_info.h"

#include "livekit_url.h"

static const char *TAG = "livekit_url";

#define URL_PARAM_SDK      "esp32"
#define URL_PARAM_VERSION  LIVEKIT_SDK_VERSION
#define URL_PARAM_OS       "idf"
#define URL_PARAM_PROTOCOL "15"
// TODO: Add protocol version parameter to URL

#define URL_FORMAT "%s%srtc?" \
    "sdk=" URL_PARAM_SDK \
    "&version=" URL_PARAM_VERSION \
    "&os=" URL_PARAM_OS \
    "&os_version=%s" \
    "&device_model=%d" \
    "&auto_subscribe=false" \
    "&protocol=" URL_PARAM_PROTOCOL \
    "&access_token=%s" // Keep at the end for log redaction

bool livekit_url_build(const char *server_url, const char *token, char **out_url)
{
    if (server_url == NULL || token == NULL || out_url == NULL) {
        return false;
    }
    size_t server_url_len = strlen(server_url);
    if (server_url_len < 1) {
        ESP_LOGE(TAG, "Server URL cannot be empty");
        return false;
    }
    if (strncmp(server_url, "ws://", 5) != 0 && strncmp(server_url, "wss://", 6) != 0) {
        ESP_LOGE(TAG, "Unsupported URL scheme");
        return false;
    }
    // Do not add a trailing slash if the URL already has one
    const char *separator = server_url[server_url_len - 1] == '/' ? "" : "/";

    // Get chip and OS information
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    int model_code = chip_info.model;
    const char* idf_version = esp_get_idf_version();

    int final_len = snprintf(NULL, 0, URL_FORMAT,
        server_url,
        separator,
        idf_version,
        model_code,
        token
    );
    *out_url = (char *)malloc(final_len + 1);
    if (*out_url == NULL) {
        return false;
    }
    sprintf(*out_url, URL_FORMAT,
        server_url,
        separator,
        idf_version,
        model_code,
        token
    );

    // Token is redacted from logging for security
    size_t token_len = strlen(token);
    ESP_LOGI(TAG, "Built signaling URL: %.*s[REDACTED]", (int)(final_len - token_len), *out_url);
    return true;
}