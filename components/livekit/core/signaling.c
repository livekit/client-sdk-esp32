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

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_netif.h"
#ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif
#include "esp_websocket_client.h"
#include "esp_tls.h"

#include "protocol.h"
#include "signaling.h"
#include "url.h"
#include "utils.h"

static const char *TAG = "livekit_signaling";

#define SIGNAL_WS_BUFFER_SIZE          20 * 1024
#define SIGNAL_WS_RECONNECT_TIMEOUT_MS 1000
#define SIGNAL_WS_NETWORK_TIMEOUT_MS   10000
#define SIGNAL_WS_CLOSE_CODE           1000
#define SIGNAL_WS_CLOSE_TIMEOUT_MS     250

typedef struct {
    esp_websocket_client_handle_t ws;
    signal_options_t options;
    signal_state_t state;
    bool is_terminal_state;
    TimerHandle_t ping_interval_timer;
    TimerHandle_t ping_timeout_timer;
    int64_t rtt;

#if CONFIG_LK_BENCHMARK
    uint64_t start_time;
#endif
} signal_t;

static inline void change_state(signal_t *sg, signal_state_t state)
{
    sg->state = state;
    sg->options.on_state_changed(state, sg->options.ctx);
}

static inline signal_state_t failed_state_from_http_status(int status)
{
    switch (status) {
        case 400: return SIGNAL_STATE_FAILED_BAD_TOKEN;
        case 401: return SIGNAL_STATE_FAILED_UNAUTHORIZED;
        default:  return status > 400 && status < 500 ?
                    SIGNAL_STATE_FAILED_CLIENT_OTHER :
                    SIGNAL_STATE_FAILED_INTERNAL;
    }
}

static signal_err_t send_request(signal_t *sg, livekit_pb_signal_request_t *request)
{
    size_t encoded_size = protocol_signal_request_encoded_size(request);
    if (encoded_size == 0) {
        return SIGNAL_ERR_MESSAGE;
    }
    uint8_t *enc_buf = (uint8_t *)malloc(encoded_size);
    if (enc_buf == NULL) {
        return SIGNAL_ERR_NO_MEM;
    }
    int ret = SIGNAL_ERR_NONE;
    do {
        if (!protocol_signal_request_encode(request, enc_buf, encoded_size)) {
            ret = SIGNAL_ERR_MESSAGE;
            break;
        }
        if (esp_websocket_client_send_bin(sg->ws,
                (const char *)enc_buf,
                (int)encoded_size,
                portMAX_DELAY) < 0) {
            //ESP_LOGE(TAG, "Failed to send request");
            ret = SIGNAL_ERR_MESSAGE;
            break;
        }
    } while (0);
    free(enc_buf);
    return ret;
}

static void on_ping_interval_expired(TimerHandle_t handle)
{
    signal_t *sg = (signal_t *)pvTimerGetTimerID(handle);

    livekit_pb_signal_request_t req = {};
    req.which_message = LIVEKIT_PB_SIGNAL_REQUEST_PING_REQ_TAG;
    req.message.ping_req.timestamp = get_unix_time_ms();
    req.message.ping_req.rtt = sg->rtt;

    send_request(sg, &req);
}

static void on_ping_timeout_expired(TimerHandle_t handle)
{
    signal_t *sg = (signal_t *)pvTimerGetTimerID(handle);
    esp_websocket_client_stop(sg->ws);
}

/// Processes responses before forwarding them to the receiver.
static inline bool res_middleware(signal_t *sg, livekit_pb_signal_response_t *res)
{
    if (res->which_message != LIVEKIT_PB_SIGNAL_RESPONSE_PONG_RESP_TAG &&
        res->which_message != LIVEKIT_PB_SIGNAL_RESPONSE_JOIN_TAG) {
        return true;
    }
    switch (res->which_message) {
        case LIVEKIT_PB_SIGNAL_RESPONSE_JOIN_TAG:
            livekit_pb_join_response_t *join = &res->message.join;
            // Calculate timer intervals and start timers: seconds -> ms, min 1s.
            int32_t ping_interval_ms = (join->ping_interval < 1 ? 1 : join->ping_interval) * 1000;
            xTimerChangePeriod(sg->ping_interval_timer, pdMS_TO_TICKS(ping_interval_ms), 0);
            xTimerStart(sg->ping_interval_timer, 0);
            int32_t ping_timeout_ms = (join->ping_timeout < 1 ? 1 : join->ping_timeout) * 1000;
            xTimerChangePeriod(sg->ping_timeout_timer, pdMS_TO_TICKS(ping_timeout_ms), 0);
            xTimerStart(sg->ping_timeout_timer, 0);
            return true;
        case LIVEKIT_PB_SIGNAL_RESPONSE_PONG_RESP_TAG:
            livekit_pb_pong_t *pong = &res->message.pong_resp;
            // Calculate round trip time (RTT) and restart ping timeout timer.
            sg->rtt = get_unix_time_ms() - pong->last_ping_timestamp;
            xTimerReset(sg->ping_timeout_timer, 0);
            return false;
        default:
            return true;
    }
}

static void on_ws_event(void *ctx, esp_event_base_t base, int32_t event_id, void *event_data)
{
    signal_t *sg = (signal_t *)ctx;
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

    switch (event_id) {
        case WEBSOCKET_EVENT_BEFORE_CONNECT:
#if CONFIG_LK_BENCHMARK
            sg->start_time = get_unix_time_ms();
#endif
            sg->is_terminal_state = false;
            change_state(sg, SIGNAL_STATE_CONNECTING);
            break;
        case WEBSOCKET_EVENT_CLOSED:
        case WEBSOCKET_EVENT_DISCONNECTED:
        case WEBSOCKET_EVENT_FINISH:
            if (sg->is_terminal_state) {
                break;
            }
            bool is_ping_timeout = xTimerIsTimerActive(sg->ping_timeout_timer) == pdFALSE;
            xTimerStop(sg->ping_timeout_timer, 0);
            xTimerStop(sg->ping_interval_timer, 0);

            if (!(sg->state & SIGNAL_STATE_FAILED_ANY)) {
                signal_state_t terminal_state = is_ping_timeout ?
                    SIGNAL_STATE_FAILED_PING_TIMEOUT :
                    SIGNAL_STATE_DISCONNECTED;
                change_state(sg, terminal_state);
            }
            sg->is_terminal_state = true;
            break;
        case WEBSOCKET_EVENT_ERROR:
            int http_status = data->error_handle.esp_ws_handshake_status_code;
            signal_state_t state = http_status != 0 ?
                failed_state_from_http_status(http_status) :
                SIGNAL_STATE_FAILED_UNREACHABLE;
            change_state(sg, state);
            break;
        case WEBSOCKET_EVENT_CONNECTED:
#if CONFIG_LK_BENCHMARK
            ESP_LOGI(TAG, "[BENCH] Connected in %" PRIu64 "ms",
                get_unix_time_ms() - sg->start_time);
#endif
            change_state(sg, SIGNAL_STATE_CONNECTED);
            break;
        case WEBSOCKET_EVENT_DATA:
            if (data->op_code != WS_TRANSPORT_OPCODES_BINARY) {
                break;
            }
            if (data->data_len < 1) break;
            livekit_pb_signal_response_t res = {};
            if (!protocol_signal_response_decode((const uint8_t *)data->data_ptr, (size_t)data->data_len, &res)) {
                break;
            }
            if (res.which_message == 0) {
                // Response type is not supported yet.
                protocol_signal_response_free(&res);
                break;
            }
            if (!res_middleware(sg, &res)) {
                // Don't forward.
                protocol_signal_response_free(&res);
                break;
            }
            if (!sg->options.on_res(&res, sg->options.ctx)) {
                // Ownership was not taken.
                protocol_signal_response_free(&res);
            }
            break;
        default:
            break;
    }
}

signal_handle_t signal_init(const signal_options_t *options)
{
    if (options == NULL ||
        options->on_state_changed == NULL ||
        options->on_res == NULL) {
        return NULL;
    }
    signal_t *sg = calloc(1, sizeof(signal_t));
    if (sg == NULL) {
        return NULL;
    }
    sg->options = *options;

    sg->ping_interval_timer = xTimerCreate(
        "ping_interval",
        pdMS_TO_TICKS(1000), // Will be overwritten before start
        pdTRUE, // Periodic
        (void *)sg,
        on_ping_interval_expired
    );
    if (sg->ping_interval_timer == NULL) {
        goto _init_failed;
    }
    sg->ping_timeout_timer = xTimerCreate(
        "ping_timeout",
        pdMS_TO_TICKS(1000), // Will be overwritten before start
        pdFALSE, // One-shot
        (void *)sg,
        on_ping_timeout_expired
    );
    if (sg->ping_timeout_timer == NULL) {
        goto _init_failed;
    }
    // URL will be set on connect
    static esp_websocket_client_config_t ws_config = {
        .buffer_size = SIGNAL_WS_BUFFER_SIZE,
        .disable_pingpong_discon = true,
        .network_timeout_ms = SIGNAL_WS_NETWORK_TIMEOUT_MS,
        .disable_auto_reconnect = true,
#ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
        .crt_bundle_attach = esp_crt_bundle_attach
#endif
    };
    sg->ws = esp_websocket_client_init(&ws_config);
    if (sg->ws == NULL) {
        goto _init_failed;
    }
    if (esp_websocket_register_events(
        sg->ws,
        WEBSOCKET_EVENT_ANY,
        on_ws_event,
        (void *)sg
    ) != ESP_OK) {
        goto _init_failed;
    }
    return sg;
_init_failed:
    signal_destroy(sg);
    return NULL;
}

signal_err_t signal_destroy(signal_handle_t handle)
{
    if (handle == NULL) {
        return SIGNAL_ERR_INVALID_ARG;
    }
    signal_t *sg = (signal_t *)handle;
    if (sg->ping_interval_timer != NULL) {
        xTimerDelete(sg->ping_interval_timer, portMAX_DELAY);
    }
    if (sg->ping_timeout_timer != NULL) {
        xTimerDelete(sg->ping_timeout_timer, portMAX_DELAY);
    }
    if (sg->ws != NULL) {
        esp_websocket_client_destroy(sg->ws);
    }
    free(sg);
    return SIGNAL_ERR_NONE;
}

signal_err_t signal_connect(signal_handle_t handle, const char* server_url, const char* token)
{
    if (server_url == NULL || token == NULL || handle == NULL) {
        return SIGNAL_ERR_INVALID_ARG;
    }
    signal_t *sg = (signal_t *)handle;

    char* url = NULL;
    url_build_options options = {
        .server_url = server_url
    };
    if (!url_build(&options, &url)) {
        return SIGNAL_ERR_INVALID_URL;
    }
    ESP_LOGI(TAG, "Connecting to server: %s", url);
    esp_websocket_client_set_uri(sg->ws, url);
    free(url);

    if (!sg->is_terminal_state) {
        // Initial connection (transport not created yet)
        char* auth_value = NULL;
        if (asprintf(&auth_value, "Bearer %s", token) < 0) {
            return SIGNAL_ERR_NO_MEM;
        }
        esp_websocket_client_append_header(sg->ws, "Authorization", auth_value);
        free(auth_value);
    } else {
        // Subsequent connection (transport already created)
        char* header_string = NULL;
        if (asprintf(&header_string, "Authorization: Bearer %s\r\n", token) < 0) {
            return SIGNAL_ERR_NO_MEM;
        }
        esp_websocket_client_set_headers(sg->ws, header_string);
        free(header_string);
    }

    if (esp_websocket_client_start(sg->ws) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WebSocket");
        return SIGNAL_ERR_WEBSOCKET;
    }
    return SIGNAL_ERR_NONE;
}

signal_err_t signal_close(signal_handle_t handle)
{
    if (handle == NULL) {
        return SIGNAL_ERR_INVALID_ARG;
    }
    signal_t *sg = (signal_t *)handle;
    if (esp_websocket_client_is_connected(sg->ws) &&
        esp_websocket_client_close(sg->ws, pdMS_TO_TICKS(SIGNAL_WS_CLOSE_TIMEOUT_MS)) != ESP_OK) {
        return SIGNAL_ERR_WEBSOCKET;
    }
    return SIGNAL_ERR_NONE;
}

signal_err_t signal_send_leave(signal_handle_t handle)
{
    if (handle == NULL) {
        return SIGNAL_ERR_INVALID_ARG;
    }
    signal_t *sg = (signal_t *)handle;
    livekit_pb_signal_request_t req = LIVEKIT_PB_SIGNAL_REQUEST_INIT_ZERO;
    req.which_message = LIVEKIT_PB_SIGNAL_REQUEST_LEAVE_TAG;

    livekit_pb_leave_request_t leave = {
        .reason = LIVEKIT_PB_DISCONNECT_REASON_CLIENT_INITIATED,
        .action = LIVEKIT_PB_LEAVE_REQUEST_ACTION_DISCONNECT
    };
    req.message.leave = leave;
    return send_request(sg, &req);
}

signal_err_t signal_send_answer(signal_handle_t handle, const char *sdp)
{
    if (sdp == NULL || handle == NULL) {
        return SIGNAL_ERR_INVALID_ARG;
    }
    signal_t *sg = (signal_t *)handle;
    livekit_pb_signal_request_t req = LIVEKIT_PB_SIGNAL_REQUEST_INIT_ZERO;

    livekit_pb_session_description_t desc = {
        .type = "answer",
        .sdp = (char *)sdp
    };
    req.which_message = LIVEKIT_PB_SIGNAL_REQUEST_ANSWER_TAG;
    req.message.answer = desc;
    return send_request(sg, &req);
}

signal_err_t signal_send_offer(signal_handle_t handle, const char *sdp)
{
    if (sdp == NULL || handle == NULL) {
        return SIGNAL_ERR_INVALID_ARG;
    }
    signal_t *sg = (signal_t *)handle;
    livekit_pb_signal_request_t req = LIVEKIT_PB_SIGNAL_REQUEST_INIT_ZERO;

    livekit_pb_session_description_t desc = {
        .type = "offer",
        .sdp = (char *)sdp
    };
    req.which_message = LIVEKIT_PB_SIGNAL_REQUEST_OFFER_TAG;
    req.message.offer = desc;
    return send_request(sg, &req);
}

signal_err_t signal_send_add_track(signal_handle_t handle, livekit_pb_add_track_request_t *add_track_req)
{
    if (handle == NULL || add_track_req == NULL) {
        return SIGNAL_ERR_INVALID_ARG;
    }
    signal_t *sg = (signal_t *)handle;
    livekit_pb_signal_request_t req = LIVEKIT_PB_SIGNAL_REQUEST_INIT_ZERO;
    req.which_message = LIVEKIT_PB_SIGNAL_REQUEST_ADD_TRACK_TAG;
    req.message.add_track = *add_track_req;
    return send_request(sg, &req);
}

signal_err_t signal_send_update_subscription(signal_handle_t handle, const char *sid, bool subscribe)
{
    if (sid == NULL || handle == NULL) {
        return SIGNAL_ERR_INVALID_ARG;
    }
    signal_t *sg = (signal_t *)handle;
    livekit_pb_signal_request_t req = LIVEKIT_PB_SIGNAL_REQUEST_INIT_ZERO;

    livekit_pb_update_subscription_t subscription = {
        .track_sids = (char*[]){(char*)sid},
        .track_sids_count = 1,
        .subscribe = subscribe
    };
    req.which_message = LIVEKIT_PB_SIGNAL_REQUEST_SUBSCRIPTION_TAG;
    req.message.subscription = subscription;
    return send_request(sg, &req);
}