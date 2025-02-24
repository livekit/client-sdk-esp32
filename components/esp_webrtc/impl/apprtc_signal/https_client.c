/**
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2025 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#include <sys/param.h>
#include <string.h>
#include "esp_log.h"
#include "https_client.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"

static const char *TAG = "HTTPS_CLIENT";

typedef struct {
    http_header_t header;
    http_body_t   body;
    uint8_t      *data;
    int           fill_size;
    int           size;
    void         *ctx;
} http_info_t;

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    http_info_t *info = evt->user_data;
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            if (info->header) {
                info->header(evt->header_key, evt->header_value, info->ctx);
            }
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key,
                     evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                int content_len = esp_http_client_get_content_length(evt->client);
                if (info->data == NULL && content_len) {
                    info->data = malloc(content_len);
                    if (info->data) {
                        info->size = content_len;
                    }
                }
                if (evt->data_len && info->fill_size + evt->data_len <= info->size) {
                    memcpy(info->data + info->fill_size, evt->data, evt->data_len);
                    info->fill_size += evt->data_len;
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            if (info->fill_size && info->body) {
                http_resp_t resp = {
                    .data = (char *)info->data,
                    .size = info->fill_size,
                };
                info->body(&resp, info->ctx);
            }
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data,
                                                             &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGD(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGD(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            break;
        case HTTP_EVENT_REDIRECT:
            esp_http_client_set_redirection(evt->client);
            break;
    }
    return ESP_OK;
}

int https_send_request(const char *method, char **headers, const char *url, char *data, http_header_t header_cb, http_body_t body, void *ctx)
{
    http_info_t info = {
        .body = body,
        .header = header_cb,
        .ctx = ctx,
    };
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .user_data = &info,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Fail to init client");
        return -1;
    }
    // POST
    int err = 0;
    esp_http_client_set_url(client, url);
    if (strcmp(method, "POST") == 0) {
        esp_http_client_set_method(client, HTTP_METHOD_POST);
    } else if (strcmp(method, "DELETE") == 0) {
        esp_http_client_set_method(client, HTTP_METHOD_DELETE);
    } else if (strcmp(method, "PATCH") == 0) {
        esp_http_client_set_method(client, HTTP_METHOD_PATCH);
    } else {
        err = -1;
        goto _exit;
    }
    bool has_content_type = false;
    if (headers) {
        int i = 0;
        // TODO suppose header writable
        while (headers[i]) {
            char *dot = strchr(headers[i], ':');
            if (dot) {
                *dot = 0;
                if (strcmp(headers[i], "Content-Type") == 0) {
                    has_content_type = true;
                }
                char *cont = dot + 2;
                esp_http_client_set_header(client, headers[i], cont);
                *dot = ':';
            }
            i++;
        }
    }
    if (data != NULL) {
        if (has_content_type == false) {
            esp_http_client_set_header(client, "Content-Type", "text/plain;charset=UTF-8");
        }
        esp_http_client_set_post_field(client, data, strlen(data));
    }
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %lld",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }
_exit:
    esp_http_client_cleanup(client);
    if (info.data) {
        free(info.data);
    }
    return err;
}

int https_post(const char *url, char **headers, char *data, http_header_t header_cb, http_body_t body, void *ctx)
{
    return https_send_request("POST", headers, url, data, header_cb, body, ctx);
}
