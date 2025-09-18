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

#include <esp_log.h>
#include <inttypes.h>
#include <khash.h>
#include "esp_timer.h"
#include "rpc_manager.h"

static const char* TAG = "livekit_rpc";

KHASH_MAP_INIT_STR(handlers, livekit_rpc_handler_t)

typedef struct {
    rpc_manager_options_t options;
    khash_t(handlers) *handlers;
} rpc_manager_t;

static bool on_result(const livekit_rpc_result_t* result, void* ctx)
{
    if (result == NULL || ctx == NULL) {
        ESP_LOGE(TAG, "Send result missing required arguments");
        return false;
    }
    rpc_manager_t *manager = (rpc_manager_t *)ctx;
    if (result->payload != NULL && strlen(result->payload) >= LIVEKIT_RPC_MAX_PAYLOAD_BYTES) {
        ESP_LOGE(TAG, "Payload too large");
        return false;
    }

    bool is_ok = result->code == LIVEKIT_RPC_RESULT_OK;
    if (is_ok && result->error_message != NULL) {
        ESP_LOGW(TAG, "Error message provided for OK result, ignoring");
    }

    // Send response packet
    livekit_pb_data_packet_t res_packet = {
        .which_value = LIVEKIT_PB_DATA_PACKET_RPC_RESPONSE_TAG
    };
    strlcpy(res_packet.value.rpc_response.request_id,
            result->id,
            sizeof(res_packet.value.rpc_response.request_id));

    if (is_ok) {
        res_packet.value.rpc_response.which_value = LIVEKIT_PB_RPC_RESPONSE_PAYLOAD_TAG;
        res_packet.value.rpc_response.value.payload = result->payload;
    } else {
        res_packet.value.rpc_response.which_value = LIVEKIT_PB_RPC_RESPONSE_ERROR_TAG;
        res_packet.value.rpc_response.value.error.code = result->code;
        res_packet.value.rpc_response.value.error.data = result->error_message;
    }
    if (!manager->options.send_packet(&res_packet, manager->options.ctx)) {
        return false;
    }
    return true;
}

static rpc_manager_err_t handle_request_packet(rpc_manager_t *manager, const livekit_pb_rpc_request_t* request, const char* caller_identity)
{
    if (caller_identity == NULL || request->method == NULL || strlen(request->id) != 36) {
        ESP_LOGD(TAG, "Invalid request packet");
        return RPC_MANAGER_ERR_NONE;
    }
    ESP_LOGD(TAG, "RPC request: method=%s, id=%s", request->method, request->id);

    livekit_pb_data_packet_t ack_packet = {
        .which_value = LIVEKIT_PB_DATA_PACKET_RPC_ACK_TAG
    };
    strlcpy(ack_packet.value.rpc_ack.request_id,
            request->id,
            sizeof(ack_packet.value.rpc_ack.request_id));

    if (!manager->options.send_packet(&ack_packet, manager->options.ctx)) {
        return RPC_MANAGER_ERR_SEND_FAILED;
    }

    if (request->version != 1) {
        ESP_LOGD(TAG, "Unsupported version: %" PRIu32, request->version);
        livekit_pb_data_packet_t res_packet = {
            .which_value = LIVEKIT_PB_DATA_PACKET_RPC_RESPONSE_TAG,
            .value.rpc_response = {
                .which_value = LIVEKIT_PB_RPC_RESPONSE_ERROR_TAG,
                .value.error = {
                    .code = LIVEKIT_RPC_RESULT_UNSUPPORTED_VERSION
                }
            }
        };
        strlcpy(res_packet.value.rpc_response.request_id,
                request->id,
                sizeof(res_packet.value.rpc_response.request_id));

        if (!manager->options.send_packet(&res_packet, manager->options.ctx)) {
            return RPC_MANAGER_ERR_SEND_FAILED;
        }
        return RPC_MANAGER_ERR_NONE;
    }

    khiter_t key = kh_get(handlers, manager->handlers, request->method);
    if (key == kh_end(manager->handlers)) {
        ESP_LOGD(TAG, "No handler registered for method '%s'", request->method);
        livekit_pb_data_packet_t res_packet = {
            .which_value = LIVEKIT_PB_DATA_PACKET_RPC_RESPONSE_TAG,
            .value.rpc_response = {
                .which_value = LIVEKIT_PB_RPC_RESPONSE_ERROR_TAG,
                .value.error = {
                    .code = LIVEKIT_RPC_RESULT_UNSUPPORTED_METHOD
                }
            }
        };
        strlcpy(res_packet.value.rpc_response.request_id,
                request->id,
                sizeof(res_packet.value.rpc_response.request_id));

        if (!manager->options.send_packet(&res_packet, manager->options.ctx)) {
            return RPC_MANAGER_ERR_SEND_FAILED;
        }
        return RPC_MANAGER_ERR_NONE;
    }
    livekit_rpc_handler_t handler = kh_value(manager->handlers, key);

    livekit_rpc_invocation_t invocation = {
        .id = (char *)request->id,
        .method = request->method,
        .caller_identity = (char *)caller_identity,
        .payload = request->payload,
        .send_result = on_result,
        .ctx = manager
    };

    // TODO: Pass through context

    int64_t start_time = esp_timer_get_time();
    handler(&invocation, NULL);

    int64_t exec_duration = esp_timer_get_time() - start_time;
    ESP_LOGD(TAG, "Handler for method '%s' took %" PRIu64 "us", request->method, exec_duration / 1000);

    // After, record should be deleted or be marked pending

    return RPC_MANAGER_ERR_NONE;
}

static rpc_manager_err_t handle_response_packet(rpc_manager_t *manager, const livekit_pb_rpc_response_t* response)
{
    // TODO: Implement
    return RPC_MANAGER_ERR_NONE;
}

static rpc_manager_err_t handle_ack_packet(rpc_manager_t *manager, const livekit_pb_rpc_ack_t* ack)
{
    // TODO: Implement
    return RPC_MANAGER_ERR_NONE;
}

rpc_manager_err_t rpc_manager_create(rpc_manager_handle_t *handle, const rpc_manager_options_t *options)
{
    if (handle  == NULL ||
        options == NULL ||
        options->on_result   == NULL ||
        options->send_packet == NULL) {
        return RPC_MANAGER_ERR_INVALID_ARG;
    }
    rpc_manager_t *rpc = (rpc_manager_t *)calloc(1, sizeof(rpc_manager_t));
    if (rpc == NULL) {
        return RPC_MANAGER_ERR_NO_MEM;
    }

    rpc->handlers = kh_init(handlers);
    if (rpc->handlers == NULL) {
        free(rpc);
        return RPC_MANAGER_ERR_NO_MEM;
    }

    rpc->options = *options;
    *handle = (rpc_manager_handle_t)rpc;
    return RPC_MANAGER_ERR_NONE;
}

rpc_manager_err_t rpc_manager_destroy(rpc_manager_handle_t handle)
{
    if (handle == NULL) {
        return RPC_MANAGER_ERR_INVALID_ARG;
    }
    rpc_manager_t *rpc = (rpc_manager_t *)handle;
    free(rpc);
    return RPC_MANAGER_ERR_NONE;
}

rpc_manager_err_t rpc_manager_register(rpc_manager_handle_t handle, const char* method, livekit_rpc_handler_t handler)
{
    if (handle == NULL || method == NULL || handler == NULL) {
        return RPC_MANAGER_ERR_INVALID_ARG;
    }
    rpc_manager_t *manager = (rpc_manager_t *)handle;

    int put_flag;
    khiter_t key = kh_put(handlers, manager->handlers, method, &put_flag);
    if (put_flag != 1) {
        return RPC_MANAGER_ERR_INVALID_STATE;
    }
    kh_value(manager->handlers, key) = handler;

    return RPC_MANAGER_ERR_NONE;
}

rpc_manager_err_t rpc_manager_unregister(rpc_manager_handle_t handle, const char* method)
{
    if (handle == NULL || method == NULL) {
        return RPC_MANAGER_ERR_INVALID_ARG;
    }
    rpc_manager_t *manager = (rpc_manager_t *)handle;

    khiter_t key = kh_get(handlers, manager->handlers, method);
    if (key == kh_end(manager->handlers)) {
        return RPC_MANAGER_ERR_INVALID_STATE;
    }
    kh_del(handlers, manager->handlers, key);

    return RPC_MANAGER_ERR_NONE;
}

rpc_manager_err_t rpc_manager_handle_packet(rpc_manager_handle_t handle, const livekit_pb_data_packet_t* packet)
{
    if (handle == NULL || packet == NULL) {
        return RPC_MANAGER_ERR_INVALID_ARG;
    }
    rpc_manager_t *manager = (rpc_manager_t *)handle;

    switch (packet->which_value) {
        case LIVEKIT_PB_DATA_PACKET_RPC_REQUEST_TAG:
            return handle_request_packet(manager, &packet->value.rpc_request, packet->participant_identity);
        case LIVEKIT_PB_DATA_PACKET_RPC_ACK_TAG:
            return handle_ack_packet(manager, &packet->value.rpc_ack);
        case LIVEKIT_PB_DATA_PACKET_RPC_RESPONSE_TAG:
            return handle_response_packet(manager, &packet->value.rpc_response);
        default:
            ESP_LOGD(TAG, "Unhandled packet type");
            return RPC_MANAGER_ERR_INVALID_STATE;
    }
    return RPC_MANAGER_ERR_NONE;
}