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

#include <stdlib.h>
#include <string.h>
#include <esp_log.h>
#include <inttypes.h>
#include <khash.h>
#include "esp_timer.h"
#include "rpc_server_manager.h"

static const char* TAG = "livekit_rpc";

#define LK_RPC_REQUEST_TOPIC                "lk.rpc_request"
#define LK_RPC_RESPONSE_TOPIC               "lk.rpc_response"
#define LK_RPC_ATTR_REQUEST_ID              "lk.rpc_request_id"
#define LK_RPC_ATTR_METHOD                  "lk.rpc_request_method"
#define LK_RPC_ATTR_RESPONSE_TIMEOUT_MS     "lk.rpc_request_response_timeout_ms"
#define LK_RPC_ATTR_VERSION                 "lk.rpc_request_version"

KHASH_MAP_INIT_STR(handlers, livekit_rpc_handler_t)

/// Per-stream state for a v2 request currently being received. Lives in
/// the @c in_flight map keyed by the stream_id; freed once on_close
/// dispatches the handler and the response has been sent.
typedef struct {
    char request_id[37];
    char *method;            // strdup'd
    char *caller_identity;   // strdup'd
    uint32_t response_timeout_ms;
    uint32_t version;
    uint8_t *payload_buf;    // accumulating
    size_t payload_size;
    size_t payload_cap;
} in_flight_request_t;

KHASH_MAP_INIT_STR(in_flight, in_flight_request_t *)

typedef struct {
    rpc_server_manager_options_t options;
    khash_t(handlers) *handlers;
    khash_t(in_flight) *in_flight;
} rpc_server_manager_t;

/// Context passed to handler invocations via @c invocation->ctx. The
/// on_result callback uses it to decide between v1 packet response and
/// v2 data stream response based on how the request arrived.
typedef struct {
    rpc_server_manager_t *manager;
    char request_id[37];
    char *caller_identity;
    bool caller_is_v2;
} invocation_ctx_t;

static void in_flight_free(in_flight_request_t *e)
{
    if (e == NULL) {
        return;
    }
    free(e->method);
    free(e->caller_identity);
    free(e->payload_buf);
    free(e);
}

/// Send an RpcAck packet for @p request_id and @p caller_identity. The
/// caller_identity is plumbed through destination_identities so the
/// server routes it back to the original caller.
static bool send_ack(rpc_server_manager_t *manager,
                     const char *request_id, const char *caller_identity)
{
    livekit_pb_data_packet_t packet = LIVEKIT_PB_DATA_PACKET_INIT_ZERO;
    packet.which_value = LIVEKIT_PB_DATA_PACKET_RPC_ACK_TAG;
    strlcpy(packet.value.rpc_ack.request_id, request_id,
            sizeof(packet.value.rpc_ack.request_id));
    char *dest[1] = { (char *)caller_identity };
    if (caller_identity != NULL) {
        packet.destination_identities = dest;
        packet.destination_identities_count = 1;
    }
    return manager->options.send_packet(&packet, manager->options.ctx);
}

/// Send a v1 RpcResponse packet carrying an error code (with optional
/// data). Used both for v1 callers and for v2 callers since error
/// responses always travel as packets even between two v2 peers.
static bool send_error_packet(rpc_server_manager_t *manager,
                              const char *request_id,
                              const char *caller_identity,
                              livekit_rpc_result_code_t code,
                              const char *message)
{
    livekit_pb_data_packet_t packet = LIVEKIT_PB_DATA_PACKET_INIT_ZERO;
    packet.which_value = LIVEKIT_PB_DATA_PACKET_RPC_RESPONSE_TAG;
    strlcpy(packet.value.rpc_response.request_id, request_id,
            sizeof(packet.value.rpc_response.request_id));
    packet.value.rpc_response.which_value = LIVEKIT_PB_RPC_RESPONSE_ERROR_TAG;
    packet.value.rpc_response.value.error.code = (uint32_t)code;
    packet.value.rpc_response.value.error.data = (char *)message;
    char *dest[1] = { (char *)caller_identity };
    if (caller_identity != NULL) {
        packet.destination_identities = dest;
        packet.destination_identities_count = 1;
    }
    return manager->options.send_packet(&packet, manager->options.ctx);
}

static bool send_success_packet(rpc_server_manager_t *manager,
                                const char *request_id,
                                const char *caller_identity,
                                const char *payload)
{
    livekit_pb_data_packet_t packet = LIVEKIT_PB_DATA_PACKET_INIT_ZERO;
    packet.which_value = LIVEKIT_PB_DATA_PACKET_RPC_RESPONSE_TAG;
    strlcpy(packet.value.rpc_response.request_id, request_id,
            sizeof(packet.value.rpc_response.request_id));
    packet.value.rpc_response.which_value = LIVEKIT_PB_RPC_RESPONSE_PAYLOAD_TAG;
    packet.value.rpc_response.value.payload = (char *)payload;
    char *dest[1] = { (char *)caller_identity };
    if (caller_identity != NULL) {
        packet.destination_identities = dest;
        packet.destination_identities_count = 1;
    }
    return manager->options.send_packet(&packet, manager->options.ctx);
}

/// Send a v2 success response over a fresh data stream on topic
/// "lk.rpc_response" with the request_id as an attribute. Returns false
/// on any failure along the way, in which case the writer slot is left
/// in whatever state the underlying writer leaves it in (each open/close
/// pair is independent).
static bool send_v2_success_stream(rpc_server_manager_t *manager,
                                   const char *request_id,
                                   const char *caller_identity,
                                   const char *payload)
{
    if (manager->options.writer == NULL) {
        return false;
    }
    const char *body = payload != NULL ? payload : "";
    size_t body_len = strlen(body);

    livekit_data_stream_attribute_t attrs[] = {
        { .key = LK_RPC_ATTR_REQUEST_ID, .value = request_id },
    };
    char *dest[1] = { (char *)caller_identity };
    livekit_data_stream_options_t opts = {
        .topic = LK_RPC_RESPONSE_TOPIC,
        .is_text = true,
        .total_length = body_len,
        .has_total_length = true,
        .attributes = attrs,
        .attributes_count = sizeof(attrs) / sizeof(attrs[0]),
        .destination_identities = dest,
        .destination_identities_count = 1,
    };
    livekit_data_stream_handle_t stream = NULL;
    if (data_stream_writer_open(manager->options.writer, &opts, &stream)
            != DATA_STREAM_WRITER_ERR_NONE) {
        return false;
    }
    if (data_stream_writer_write(manager->options.writer, stream,
                                 (const uint8_t *)body, body_len)
            != DATA_STREAM_WRITER_ERR_NONE) {
        data_stream_writer_close(manager->options.writer, stream);
        return false;
    }
    return data_stream_writer_close(manager->options.writer, stream)
        == DATA_STREAM_WRITER_ERR_NONE;
}

static bool on_result(const livekit_rpc_result_t* result, void* ctx)
{
    if (result == NULL || ctx == NULL) {
        ESP_LOGE(TAG, "Send result missing required arguments");
        return false;
    }
    invocation_ctx_t *inv_ctx = (invocation_ctx_t *)ctx;
    rpc_server_manager_t *manager = inv_ctx->manager;

    bool is_ok = result->code == LIVEKIT_RPC_RESULT_OK;
    if (is_ok && result->error_message != NULL) {
        ESP_LOGW(TAG, "Error message provided for OK result, ignoring");
    }

    if (!is_ok) {
        // Error responses always travel as v1 RpcResponse packets, even
        // when both peers are v2 (spec mandate).
        return send_error_packet(manager, result->id, inv_ctx->caller_identity,
                                 result->code, result->error_message);
    }

    if (inv_ctx->caller_is_v2) {
        // v2 success: send over a data stream. No 15 KB cap.
        return send_v2_success_stream(manager, result->id,
                                      inv_ctx->caller_identity,
                                      result->payload);
    }

    // v1 success: send packet. 15 KB cap applies on the v1 path.
    if (result->payload != NULL && strlen(result->payload) >= LIVEKIT_RPC_MAX_PAYLOAD_BYTES) {
        ESP_LOGE(TAG, "v1 success payload too large");
        return false;
    }
    return send_success_packet(manager, result->id, inv_ctx->caller_identity,
                               result->payload);
}

/// Look up the handler for @p method and invoke it. @p payload is a
/// NUL-terminated UTF-8 string. Caller_is_v2 indicates whether this
/// request arrived as a v2 data stream (drives response transport).
static void dispatch_to_handler(rpc_server_manager_t *manager,
                                const char *request_id,
                                const char *method,
                                const char *caller_identity,
                                const char *payload,
                                bool caller_is_v2)
{
    khiter_t k = kh_get(handlers, manager->handlers, method);
    if (k == kh_end(manager->handlers)) {
        ESP_LOGD(TAG, "No handler registered for method '%s'", method);
        send_error_packet(manager, request_id, caller_identity,
                          LIVEKIT_RPC_RESULT_UNSUPPORTED_METHOD, NULL);
        return;
    }
    livekit_rpc_handler_t handler = kh_value(manager->handlers, k);

    invocation_ctx_t inv_ctx = {
        .manager = manager,
        .caller_identity = (char *)caller_identity,
        .caller_is_v2 = caller_is_v2,
    };
    strlcpy(inv_ctx.request_id, request_id, sizeof(inv_ctx.request_id));

    livekit_rpc_invocation_t invocation = {
        .id = inv_ctx.request_id,
        .method = (char *)method,
        .caller_identity = (char *)caller_identity,
        .payload = (char *)payload,
        .send_result = on_result,
        .ctx = &inv_ctx,
    };
    int64_t start_time = esp_timer_get_time();
    handler(&invocation, NULL);
    int64_t exec_duration = esp_timer_get_time() - start_time;
    ESP_LOGD(TAG, "Handler for method '%s' took %" PRIu64 "us",
             method, exec_duration / 1000);
}

static rpc_server_manager_err_t handle_request_packet(rpc_server_manager_t *manager, const livekit_pb_rpc_request_t* request, const char* caller_identity)
{
    if (caller_identity == NULL || request->method == NULL || strlen(request->id) != 36) {
        ESP_LOGD(TAG, "Invalid request packet");
        return RPC_SERVER_MANAGER_ERR_NONE;
    }
    ESP_LOGD(TAG, "RPC v1 request: method=%s, id=%s", request->method, request->id);

    if (!send_ack(manager, request->id, caller_identity)) {
        return RPC_SERVER_MANAGER_ERR_SEND_FAILED;
    }

    if (request->version != 1) {
        ESP_LOGD(TAG, "Unsupported version: %" PRIu32, request->version);
        send_error_packet(manager, request->id, caller_identity,
                          LIVEKIT_RPC_RESULT_UNSUPPORTED_VERSION, NULL);
        return RPC_SERVER_MANAGER_ERR_NONE;
    }
    dispatch_to_handler(manager, request->id, request->method,
                        caller_identity, request->payload,
                        false /* caller_is_v2 */);
    return RPC_SERVER_MANAGER_ERR_NONE;
}

rpc_server_manager_err_t rpc_server_manager_create(rpc_server_manager_handle_t *handle, const rpc_server_manager_options_t *options)
{
    if (handle  == NULL ||
        options == NULL ||
        options->on_result   == NULL ||
        options->send_packet == NULL) {
        return RPC_SERVER_MANAGER_ERR_INVALID_ARG;
    }
    rpc_server_manager_t *rpc = (rpc_server_manager_t *)calloc(1, sizeof(rpc_server_manager_t));
    if (rpc == NULL) {
        return RPC_SERVER_MANAGER_ERR_NO_MEM;
    }

    rpc->handlers = kh_init(handlers);
    if (rpc->handlers == NULL) {
        free(rpc);
        return RPC_SERVER_MANAGER_ERR_NO_MEM;
    }
    rpc->in_flight = kh_init(in_flight);
    if (rpc->in_flight == NULL) {
        kh_destroy(handlers, rpc->handlers);
        free(rpc);
        return RPC_SERVER_MANAGER_ERR_NO_MEM;
    }

    rpc->options = *options;
    *handle = (rpc_server_manager_handle_t)rpc;
    return RPC_SERVER_MANAGER_ERR_NONE;
}

rpc_server_manager_err_t rpc_server_manager_destroy(rpc_server_manager_handle_t handle)
{
    if (handle == NULL) {
        return RPC_SERVER_MANAGER_ERR_INVALID_ARG;
    }
    rpc_server_manager_t *rpc = (rpc_server_manager_t *)handle;
    if (rpc->in_flight != NULL) {
        for (khiter_t k = kh_begin(rpc->in_flight); k != kh_end(rpc->in_flight); ++k) {
            if (!kh_exist(rpc->in_flight, k)) {
                continue;
            }
            free((void *)kh_key(rpc->in_flight, k));
            in_flight_free(kh_value(rpc->in_flight, k));
        }
        kh_destroy(in_flight, rpc->in_flight);
    }
    if (rpc->handlers != NULL) {
        kh_destroy(handlers, rpc->handlers);
    }
    free(rpc);
    return RPC_SERVER_MANAGER_ERR_NONE;
}

rpc_server_manager_err_t rpc_server_manager_register(rpc_server_manager_handle_t handle, const char* method, livekit_rpc_handler_t handler)
{
    if (handle == NULL || method == NULL || handler == NULL) {
        return RPC_SERVER_MANAGER_ERR_INVALID_ARG;
    }
    rpc_server_manager_t *manager = (rpc_server_manager_t *)handle;

    int put_flag;
    khiter_t key = kh_put(handlers, manager->handlers, method, &put_flag);
    if (put_flag != 1) {
        return RPC_SERVER_MANAGER_ERR_INVALID_STATE;
    }
    kh_value(manager->handlers, key) = handler;

    return RPC_SERVER_MANAGER_ERR_NONE;
}

rpc_server_manager_err_t rpc_server_manager_unregister(rpc_server_manager_handle_t handle, const char* method)
{
    if (handle == NULL || method == NULL) {
        return RPC_SERVER_MANAGER_ERR_INVALID_ARG;
    }
    rpc_server_manager_t *manager = (rpc_server_manager_t *)handle;

    khiter_t key = kh_get(handlers, manager->handlers, method);
    if (key == kh_end(manager->handlers)) {
        return RPC_SERVER_MANAGER_ERR_INVALID_STATE;
    }
    kh_del(handlers, manager->handlers, key);

    return RPC_SERVER_MANAGER_ERR_NONE;
}

rpc_server_manager_err_t rpc_server_manager_handle_packet(rpc_server_manager_handle_t handle, const livekit_pb_data_packet_t* packet)
{
    if (handle == NULL || packet == NULL) {
        return RPC_SERVER_MANAGER_ERR_INVALID_ARG;
    }
    rpc_server_manager_t *manager = (rpc_server_manager_t *)handle;

    if (packet->which_value == LIVEKIT_PB_DATA_PACKET_RPC_REQUEST_TAG) {
        return handle_request_packet(manager, &packet->value.rpc_request,
                                     packet->participant_identity);
    }
    // Ack and response packets are caller-side concerns and now belong
    // to rpc_client_manager.
    ESP_LOGD(TAG, "Unhandled packet type %d", packet->which_value);
    return RPC_SERVER_MANAGER_ERR_NONE;
}

// MARK: - v2 request stream handlers

static const char *find_attribute_value(const livekit_data_stream_header_t *header,
                                        const char *key)
{
    if (header == NULL || header->attributes == NULL || key == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < header->attributes_count; i++) {
        const char *k = header->attributes[i].key;
        if (k != NULL && strcmp(k, key) == 0) {
            return header->attributes[i].value;
        }
    }
    return NULL;
}

static bool append_request_bytes(in_flight_request_t *entry,
                                 const uint8_t *data, size_t size)
{
    if (size == 0) {
        return true;
    }
    size_t needed = entry->payload_size + size;
    if (needed > entry->payload_cap) {
        size_t new_cap = entry->payload_cap == 0 ? 256 : entry->payload_cap * 2;
        while (new_cap < needed) {
            new_cap *= 2;
        }
        uint8_t *grown = realloc(entry->payload_buf, new_cap);
        if (grown == NULL) {
            return false;
        }
        entry->payload_buf = grown;
        entry->payload_cap = new_cap;
    }
    memcpy(entry->payload_buf + entry->payload_size, data, size);
    entry->payload_size += size;
    return true;
}

void rpc_server_manager_on_request_stream_open(rpc_server_manager_handle_t handle,
                                               const livekit_data_stream_header_t *header)
{
    if (handle == NULL || header == NULL || header->sender_identity == NULL) {
        return;
    }
    rpc_server_manager_t *manager = (rpc_server_manager_t *)handle;

    const char *request_id = find_attribute_value(header, LK_RPC_ATTR_REQUEST_ID);
    const char *method = find_attribute_value(header, LK_RPC_ATTR_METHOD);
    const char *version_str = find_attribute_value(header, LK_RPC_ATTR_VERSION);
    const char *timeout_str = find_attribute_value(header, LK_RPC_ATTR_RESPONSE_TIMEOUT_MS);
    if (request_id == NULL || strlen(request_id) != 36 || method == NULL) {
        ESP_LOGD(TAG, "v2 request stream %s missing required attributes",
                 header->stream_id);
        return;
    }

    // Send the ack BEFORE any handler lookup or validation so the caller's
    // ack contract is honored regardless of whether the method exists
    // (spec test 5).
    if (!send_ack(manager, request_id, header->sender_identity)) {
        ESP_LOGW(TAG, "failed to send ack for v2 request %s", request_id);
    }

    uint32_t version = version_str != NULL ? (uint32_t)atoi(version_str) : 0;
    if (version != 2) {
        ESP_LOGD(TAG, "v2 request with unsupported version=%s", version_str ? version_str : "(null)");
        send_error_packet(manager, request_id, header->sender_identity,
                          LIVEKIT_RPC_RESULT_UNSUPPORTED_VERSION, NULL);
        return;
    }

    in_flight_request_t *entry = calloc(1, sizeof(*entry));
    if (entry == NULL) {
        ESP_LOGE(TAG, "out of memory allocating in-flight v2 request");
        return;
    }
    strlcpy(entry->request_id, request_id, sizeof(entry->request_id));
    entry->method = strdup(method);
    entry->caller_identity = strdup(header->sender_identity);
    if (entry->method == NULL || entry->caller_identity == NULL) {
        in_flight_free(entry);
        return;
    }
    entry->version = version;
    entry->response_timeout_ms = timeout_str != NULL
        ? (uint32_t)strtoul(timeout_str, NULL, 10)
        : 0;

    char *owned_key = strdup(header->stream_id);
    if (owned_key == NULL) {
        in_flight_free(entry);
        return;
    }
    int put_flag = 0;
    khiter_t k = kh_put(in_flight, manager->in_flight, owned_key, &put_flag);
    if (put_flag < 0) {
        free(owned_key);
        in_flight_free(entry);
        return;
    }
    if (put_flag == 0) {
        // Already in the map? Replace it and free the old entry.
        in_flight_free(kh_value(manager->in_flight, k));
        free(owned_key); // existing key is reused
    }
    kh_value(manager->in_flight, k) = entry;
}

void rpc_server_manager_on_request_stream_chunk(rpc_server_manager_handle_t handle,
                                                const livekit_data_stream_chunk_t *chunk)
{
    if (handle == NULL || chunk == NULL) {
        return;
    }
    rpc_server_manager_t *manager = (rpc_server_manager_t *)handle;
    khiter_t k = kh_get(in_flight, manager->in_flight, chunk->stream_id);
    if (k == kh_end(manager->in_flight)) {
        return;
    }
    in_flight_request_t *entry = kh_value(manager->in_flight, k);
    if (chunk->content != NULL && chunk->content_size > 0) {
        if (!append_request_bytes(entry, chunk->content, chunk->content_size)) {
            ESP_LOGE(TAG, "out of memory accumulating v2 request %s",
                     entry->request_id);
        }
    }
}

void rpc_server_manager_on_request_stream_close(rpc_server_manager_handle_t handle,
                                                const livekit_data_stream_trailer_t *trailer)
{
    if (handle == NULL || trailer == NULL) {
        return;
    }
    rpc_server_manager_t *manager = (rpc_server_manager_t *)handle;
    khiter_t k = kh_get(in_flight, manager->in_flight, trailer->stream_id);
    if (k == kh_end(manager->in_flight)) {
        return;
    }
    in_flight_request_t *entry = kh_value(manager->in_flight, k);
    const char *key = kh_key(manager->in_flight, k);
    kh_del(in_flight, manager->in_flight, k);

    char *payload = malloc(entry->payload_size + 1);
    if (payload == NULL) {
        send_error_packet(manager, entry->request_id, entry->caller_identity,
                          LIVEKIT_RPC_RESULT_APPLICATION, "out of memory");
    } else {
        if (entry->payload_size > 0 && entry->payload_buf != NULL) {
            memcpy(payload, entry->payload_buf, entry->payload_size);
        }
        payload[entry->payload_size] = '\0';
        dispatch_to_handler(manager, entry->request_id, entry->method,
                            entry->caller_identity, payload,
                            true /* caller_is_v2 */);
        free(payload);
    }

    free((void *)key);
    in_flight_free(entry);
}
