#include <stdio.h>
#include <string.h>
#include "esp_log.h"

#include "livekit_url.h"

static const char *TAG = "livekit_url";

#define URL_PARAM_PROTOCOL "15"
#define URL_PARAM_SDK      "esp32"
#define URL_PARAM_VERSION  "alpha"

bool livekit_url_build(const char *server_url, const char *token, char **out_url)
{
    static const char url_format[] = "%s%srtc?sdk=%s&version=%s&auto_subscribe=true&access_token=%s";
    // TODO: Add protocol version parameter
    // Access token parameter must stay at the end for logging

    if (server_url == NULL || token == NULL || out_url == NULL) {
        return false;
    }
    size_t server_url_len = strlen(server_url);
    if (server_url_len < 1) {
        ESP_LOGE(TAG, "URL cannot be empty");
        return false;
    }

    if (strncmp(server_url, "ws://", 5) != 0 && strncmp(server_url, "wss://", 6) != 0) {
        ESP_LOGE(TAG, "Unsupported URL scheme");
        return false;
    }
    // Do not add a trailing slash if the URL already has one
    const char *separator = server_url[server_url_len - 1] == '/' ? "" : "/";

    int final_len = snprintf(NULL, 0, url_format,
        server_url,
        separator,
        URL_PARAM_SDK,
        URL_PARAM_VERSION,
        token
    );

    *out_url = (char *)malloc(final_len + 1);
    if (*out_url == NULL) {
        return false;
    }

    sprintf(*out_url, url_format,
        server_url,
        separator,
        URL_PARAM_SDK,
        URL_PARAM_VERSION,
        token
    );

    // Token is redacted from logging for security
    size_t token_len = strlen(token);
    ESP_LOGI(TAG, "Built signaling URL: %.*s[REDACTED]", (int)(final_len - token_len), *out_url);
    return true;
}