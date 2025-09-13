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

#include "cJSON.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include <sys/param.h>
#ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif

#include "livekit_sandbox.h"

static const char *TAG = "livekit_sandbox";
static const char *SANDBOX_URL = "http://cloud-api.livekit.io/api/sandbox/connection-details";

#define MAX_HTTP_OUTPUT_BUFFER 2048

static esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    // TODO: This should probably be made non-static.
    static int output_len = 0;
    char* res_buffer = (char *) evt->user_data;

    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
        case HTTP_EVENT_ON_FINISH:
        case HTTP_EVENT_DISCONNECTED:
            output_len = 0;
            break;
        case HTTP_EVENT_ON_DATA:
            assert(evt->user_data != NULL);
            if (output_len == 0) {
                memset(res_buffer, 0, MAX_HTTP_OUTPUT_BUFFER);
            }
            if (esp_http_client_is_chunked_response(evt->client)) break;

            int copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
            if (copy_len > 0) {
                memcpy(res_buffer + output_len, evt->data, copy_len);
            }
            output_len += copy_len;
            break;
        case HTTP_EVENT_REDIRECT:
            esp_http_client_set_redirection(evt->client);
            break;
        default:
            break;
    }
    return ESP_OK;
}

bool livekit_sandbox_generate(const livekit_sandbox_options_t *options, livekit_sandbox_res_t *res)
{
    if (options == NULL || options->sandbox_id == NULL) {
        ESP_LOGE(TAG, "Missing required options");
        return false;
    }

    char* res_buffer = calloc(MAX_HTTP_OUTPUT_BUFFER + 1, sizeof(char));
    if (res_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate response buffer");
        return false;
    }

    // Create JSON payload
    cJSON *json_payload = cJSON_CreateObject();
    if (json_payload == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON payload");
        free(res_buffer);
        return false;
    }
    if (options->room_name != NULL) {
        cJSON_AddStringToObject(json_payload, "roomName", options->room_name);
    }
    if (options->participant_name != NULL) {
        cJSON_AddStringToObject(json_payload, "participantName", options->participant_name);
    }
    char *json_string = cJSON_Print(json_payload);
    if (json_string == NULL) {
        ESP_LOGE(TAG, "Failed to serialize JSON payload");
        cJSON_Delete(json_payload);
        free(res_buffer);
        return false;
    }

    esp_http_client_config_t http_config = {
        .url = SANDBOX_URL,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
        .event_handler = _http_event_handler,
        .user_data = res_buffer,
#ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
        .crt_bundle_attach = esp_crt_bundle_attach
#endif
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to create HTTP client");
        free(json_string);
        cJSON_Delete(json_payload);
        free(res_buffer);
        return false;
    }

    // Set headers and POST data
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "X-Sandbox-ID", options->sandbox_id);
    esp_http_client_set_post_field(client, json_string, strlen(json_string));

    bool success = false;
    cJSON *res_json = NULL;
    do {
        esp_err_t perform_err = esp_http_client_perform(client);
        if (perform_err != ESP_OK) {
            ESP_LOGE(TAG, "Request failed: %s", esp_err_to_name(perform_err));
            break;
        }

        int status_code = esp_http_client_get_status_code(client);
        if (status_code != 200) {
            ESP_LOGE(TAG, "Request failed with status %d", status_code);
            if (strlen(res_buffer) > 0) {
                ESP_LOGE(TAG, "Server response: %s", res_buffer);
            }
            break;
        }

        res_json = cJSON_Parse(res_buffer);
        if (res_json == NULL) {
            const char *error_ptr = cJSON_GetErrorPtr();
            if (error_ptr != NULL)
                ESP_LOGE(TAG, "Failed to parse response: %s", error_ptr);
            break;
        }

        cJSON *server_url = cJSON_GetObjectItemCaseSensitive(res_json, "serverUrl");
        if (!cJSON_IsString(server_url) || (server_url->valuestring == NULL)) {
            ESP_LOGE(TAG, "Missing server URL in response");
            break;
        }
        cJSON *token = cJSON_GetObjectItemCaseSensitive(res_json, "participantToken");
        if (!cJSON_IsString(token) || (token->valuestring == NULL)) {
            ESP_LOGE(TAG, "Missing token in response");
            break;
        }
        cJSON *room_name_resp = cJSON_GetObjectItemCaseSensitive(res_json, "roomName");
        if (!cJSON_IsString(room_name_resp) || (room_name_resp->valuestring == NULL)) {
            ESP_LOGE(TAG, "Missing room name in response");
            break;
        }
        cJSON *participant_name_resp = cJSON_GetObjectItemCaseSensitive(res_json, "participantName");
        if (!cJSON_IsString(participant_name_resp) || (participant_name_resp->valuestring == NULL)) {
            ESP_LOGE(TAG, "Missing participant name in response");
            break;
        }

        res->server_url = strdup(server_url->valuestring);
        res->token = strdup(token->valuestring);
        res->room_name = strdup(room_name_resp->valuestring);
        res->participant_name = strdup(participant_name_resp->valuestring);
        success = true;

        ESP_LOGI(TAG, "Generated sandbox token\nroom_name=%s\nparticipant_name=%s",
            res->room_name, res->participant_name);
    } while (0);

    esp_http_client_cleanup(client);
    cJSON_Delete(res_json);
    cJSON_Delete(json_payload);
    free(json_string);
    free(res_buffer);
    return success;
}

void livekit_sandbox_res_free(livekit_sandbox_res_t *result)
{
    if (result == NULL) return;
    if (result->server_url) free(result->server_url);
    if (result->token) free(result->token);
    if (result->room_name) free(result->room_name);
    if (result->participant_name) free(result->participant_name);
}