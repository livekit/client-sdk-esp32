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
#include <stdlib.h>
#include <string.h>
#include <esp_log.h>
#include <khash.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "utils.h"
#include "rpc_client_manager.h"

static const char* TAG = "livekit_rpc_client";

/// Default response timeout if the caller passes 0.
#define DEFAULT_RESPONSE_TIMEOUT_MS 10000

/// Time we are willing to wait for a timer-service command to be queued.
#define TIMER_CMD_TICKS pdMS_TO_TICKS(100)

#define LK_RPC_REQUEST_TOPIC                 "lk.rpc_request"
#define LK_RPC_RESPONSE_TOPIC                "lk.rpc_response"
#define LK_RPC_ATTR_REQUEST_ID               "lk.rpc_request_id"
#define LK_RPC_ATTR_METHOD                   "lk.rpc_request_method"
#define LK_RPC_ATTR_RESPONSE_TIMEOUT_MS      "lk.rpc_request_response_timeout_ms"
#define LK_RPC_ATTR_VERSION                  "lk.rpc_request_version"
#define LK_RPC_VERSION_V2                    "2"

typedef struct rpc_client_manager rpc_client_manager_t;
typedef struct pending_request pending_request_t;

/// Carrier passed as the FreeRTOS timer's pvTimerID. Allocated separately
/// from the pending_request so the FreeRTOS timer service can keep its
/// pointer valid until the timer is fully deleted.
typedef struct {
    char request_id[37];
    rpc_client_manager_t *mgr;
    bool is_ack_timeout;
} timeout_arg_t;

struct pending_request {
    char request_id[37];
    char *destination_identity;
    TimerHandle_t ack_timer;
    TimerHandle_t response_timer;
    timeout_arg_t *ack_arg;
    timeout_arg_t *response_arg;
    bool ack_received;
    /// True if the request was sent via the v2 data-stream transport.
    /// When set, the manager looks for the response on the
    /// "lk.rpc_response" data stream rather than as an RpcResponse packet.
    bool is_v2;
    /// Stream id of the bound response data stream (empty until the
    /// stream's header arrives and is matched to this entry). Used to
    /// route subsequent chunks/trailer.
    char response_stream_id[37];
    /// Heap-allocated accumulator for the v2 response payload.
    uint8_t *response_buf;
    size_t response_buf_size;
    size_t response_buf_cap;
};

KHASH_MAP_INIT_STR(pending, pending_request_t *)

struct rpc_client_manager {
    rpc_client_manager_options_t options;
    khash_t(pending) *pending;
    SemaphoreHandle_t mutex;
};

/// Drop the entry from the map (caller holds the mutex). Returns the
/// owning pointer; caller is responsible for freeing it via free_pending().
static pending_request_t *take_entry_locked(rpc_client_manager_t *mgr, const char *request_id)
{
    khiter_t k = kh_get(pending, mgr->pending, request_id);
    if (k == kh_end(mgr->pending)) {
        return NULL;
    }
    const char *key = kh_key(mgr->pending, k);
    pending_request_t *entry = kh_value(mgr->pending, k);
    kh_del(pending, mgr->pending, k);
    free((void *)key);
    return entry;
}

/// Stop+delete both timers and free the entry. The timer service handles
/// queued delete commands after the calling context unwinds, so this is
/// safe to call from a timer callback.
static void free_pending(pending_request_t *entry)
{
    if (entry == NULL) {
        return;
    }
    if (entry->ack_timer != NULL) {
        xTimerStop(entry->ack_timer, TIMER_CMD_TICKS);
        xTimerDelete(entry->ack_timer, TIMER_CMD_TICKS);
    }
    if (entry->response_timer != NULL) {
        xTimerStop(entry->response_timer, TIMER_CMD_TICKS);
        xTimerDelete(entry->response_timer, TIMER_CMD_TICKS);
    }
    free(entry->ack_arg);
    free(entry->response_arg);
    free(entry->destination_identity);
    free(entry->response_buf);
    free(entry);
}

static void deliver_result(rpc_client_manager_t *mgr, const char *request_id,
                           livekit_rpc_result_code_t code,
                           const char *payload, const char *error_message)
{
    if (mgr->options.on_result == NULL) {
        return;
    }
    livekit_rpc_result_t result = {
        .id = (char *)request_id,
        .code = code,
        .payload = (char *)payload,
        .error_message = (char *)error_message,
    };
    mgr->options.on_result(&result, mgr->options.ctx);
}

static void timeout_cb(TimerHandle_t timer)
{
    timeout_arg_t *arg = (timeout_arg_t *)pvTimerGetTimerID(timer);
    if (arg == NULL || arg->mgr == NULL) {
        return;
    }
    rpc_client_manager_t *mgr = arg->mgr;
    char request_id[37];
    strlcpy(request_id, arg->request_id, sizeof(request_id));
    bool is_ack = arg->is_ack_timeout;

    pending_request_t *entry = NULL;
    bool should_resolve = false;
    if (xSemaphoreTake(mgr->mutex, portMAX_DELAY) == pdTRUE) {
        khiter_t k = kh_get(pending, mgr->pending, request_id);
        if (k != kh_end(mgr->pending)) {
            pending_request_t *e = kh_value(mgr->pending, k);
            if (is_ack && e->ack_received) {
                // The ack arrived; this is a stale firing of a not-yet-stopped
                // timer. Leave the entry alone; the response side will clean up.
            } else {
                const char *key = kh_key(mgr->pending, k);
                kh_del(pending, mgr->pending, k);
                free((void *)key);
                entry = e;
                should_resolve = true;
            }
        }
        xSemaphoreGive(mgr->mutex);
    }

    if (!should_resolve) {
        return;
    }

    deliver_result(mgr, request_id,
                   is_ack ? LIVEKIT_RPC_RESULT_CONNECTION_TIMEOUT
                          : LIVEKIT_RPC_RESULT_RESPONSE_TIMEOUT,
                   NULL, NULL);
    free_pending(entry);
}

static TimerHandle_t create_timer(rpc_client_manager_t *mgr,
                                  const char *request_id, bool is_ack,
                                  uint32_t timeout_ms,
                                  timeout_arg_t **out_arg)
{
    timeout_arg_t *arg = calloc(1, sizeof(*arg));
    if (arg == NULL) {
        return NULL;
    }
    strlcpy(arg->request_id, request_id, sizeof(arg->request_id));
    arg->mgr = mgr;
    arg->is_ack_timeout = is_ack;

    TickType_t ticks = pdMS_TO_TICKS(timeout_ms);
    if (ticks == 0) {
        ticks = 1; // FreeRTOS rejects 0-tick timers
    }
    TimerHandle_t timer = xTimerCreate(
        is_ack ? "rpc-ack" : "rpc-resp",
        ticks,
        pdFALSE, // one-shot
        arg,
        timeout_cb);
    if (timer == NULL) {
        free(arg);
        return NULL;
    }
    *out_arg = arg;
    return timer;
}

rpc_client_manager_err_t rpc_client_manager_create(
    rpc_client_manager_handle_t *handle,
    const rpc_client_manager_options_t *options)
{
    if (handle == NULL || options == NULL ||
        options->send_packet == NULL ||
        options->on_result == NULL ||
        options->get_peer_protocol == NULL) {
        return RPC_CLIENT_MANAGER_ERR_INVALID_ARG;
    }
    rpc_client_manager_t *mgr = calloc(1, sizeof(*mgr));
    if (mgr == NULL) {
        return RPC_CLIENT_MANAGER_ERR_NO_MEM;
    }
    mgr->pending = kh_init(pending);
    if (mgr->pending == NULL) {
        free(mgr);
        return RPC_CLIENT_MANAGER_ERR_NO_MEM;
    }
    mgr->mutex = xSemaphoreCreateMutex();
    if (mgr->mutex == NULL) {
        kh_destroy(pending, mgr->pending);
        free(mgr);
        return RPC_CLIENT_MANAGER_ERR_NO_MEM;
    }
    mgr->options = *options;
    *handle = (rpc_client_manager_handle_t)mgr;
    return RPC_CLIENT_MANAGER_ERR_NONE;
}

rpc_client_manager_err_t rpc_client_manager_destroy(rpc_client_manager_handle_t handle)
{
    if (handle == NULL) {
        return RPC_CLIENT_MANAGER_ERR_INVALID_ARG;
    }
    rpc_client_manager_t *mgr = (rpc_client_manager_t *)handle;

    // Drain all pending entries, failing each with RECIPIENT_DISCONNECTED.
    if (xSemaphoreTake(mgr->mutex, portMAX_DELAY) == pdTRUE) {
        for (khiter_t k = kh_begin(mgr->pending); k != kh_end(mgr->pending); ++k) {
            if (!kh_exist(mgr->pending, k)) {
                continue;
            }
            const char *key = kh_key(mgr->pending, k);
            pending_request_t *entry = kh_value(mgr->pending, k);
            kh_del(pending, mgr->pending, k);
            free((void *)key);
            // Deliver result without holding the mutex would be safer, but
            // on shutdown we accept the simpler path; users should not call
            // back into the manager from on_result during destroy.
            deliver_result(mgr, entry->request_id,
                           LIVEKIT_RPC_RESULT_RECIPIENT_DISCONNECTED, NULL, NULL);
            free_pending(entry);
        }
        xSemaphoreGive(mgr->mutex);
    }
    kh_destroy(pending, mgr->pending);
    vSemaphoreDelete(mgr->mutex);
    free(mgr);
    return RPC_CLIENT_MANAGER_ERR_NONE;
}

rpc_client_manager_err_t rpc_client_manager_invoke(
    rpc_client_manager_handle_t handle,
    const livekit_rpc_invoke_options_t *options)
{
    if (handle == NULL || options == NULL ||
        options->destination_identity == NULL ||
        options->method == NULL ||
        options->payload == NULL) {
        return RPC_CLIENT_MANAGER_ERR_INVALID_ARG;
    }
    rpc_client_manager_t *mgr = (rpc_client_manager_t *)handle;

    uint32_t response_timeout_ms = options->response_timeout_ms != 0
        ? options->response_timeout_ms
        : DEFAULT_RESPONSE_TIMEOUT_MS;

    char request_id[37];
    generate_uuid(request_id);

    int peer_protocol = mgr->options.get_peer_protocol(
        options->destination_identity, mgr->options.ctx);
    bool use_v2 = (peer_protocol == LIVEKIT_CLIENT_PROTOCOL_DATA_STREAM_RPC)
                  && (mgr->options.writer != NULL);

    if (!use_v2 && strlen(options->payload) >= LIVEKIT_RPC_MAX_PAYLOAD_BYTES) {
        ESP_LOGW(TAG, "request payload too large for v1 peer (%zu bytes)",
                 strlen(options->payload));
        deliver_result(mgr, request_id,
                       LIVEKIT_RPC_RESULT_REQUEST_PAYLOAD_TOO_LARGE, NULL, NULL);
        return RPC_CLIENT_MANAGER_ERR_NONE;
    }

    // Allocate the pending entry up front; ownership is tracked carefully
    // so any failure path below can free it without leaking.
    pending_request_t *entry = calloc(1, sizeof(*entry));
    if (entry == NULL) {
        return RPC_CLIENT_MANAGER_ERR_NO_MEM;
    }
    strlcpy(entry->request_id, request_id, sizeof(entry->request_id));
    entry->is_v2 = use_v2;
    entry->destination_identity = strdup(options->destination_identity);
    if (entry->destination_identity == NULL) {
        free(entry);
        return RPC_CLIENT_MANAGER_ERR_NO_MEM;
    }
    entry->ack_timer = create_timer(mgr, request_id, true,
                                    LIVEKIT_RPC_MAX_ROUND_TRIP_MS, &entry->ack_arg);
    entry->response_timer = create_timer(mgr, request_id, false,
                                         response_timeout_ms, &entry->response_arg);
    if (entry->ack_timer == NULL || entry->response_timer == NULL) {
        free_pending(entry);
        return RPC_CLIENT_MANAGER_ERR_NO_MEM;
    }

    // Insert the entry into the map *before* sending so that a response
    // delivered synchronously during the publish path can match. The mutex
    // is released before send_packet so the response handler can take it.
    char *owned_key = strdup(request_id);
    if (owned_key == NULL) {
        free_pending(entry);
        return RPC_CLIENT_MANAGER_ERR_NO_MEM;
    }
    if (xSemaphoreTake(mgr->mutex, portMAX_DELAY) != pdTRUE) {
        free(owned_key);
        free_pending(entry);
        return RPC_CLIENT_MANAGER_ERR_INVALID_STATE;
    }
    int put_flag = 0;
    khiter_t k = kh_put(pending, mgr->pending, owned_key, &put_flag);
    if (put_flag < 0) {
        xSemaphoreGive(mgr->mutex);
        free(owned_key);
        free_pending(entry);
        return RPC_CLIENT_MANAGER_ERR_NO_MEM;
    }
    kh_value(mgr->pending, k) = entry;
    xTimerStart(entry->ack_timer, TIMER_CMD_TICKS);
    xTimerStart(entry->response_timer, TIMER_CMD_TICKS);
    xSemaphoreGive(mgr->mutex);

    bool sent = false;
    if (use_v2) {
        // v2 transport: write request payload to a text data stream on
        // topic "lk.rpc_request" with RPC metadata in attributes. The
        // handler side still sends an RpcAck packet, so the ack timer
        // logic above is unchanged.
        char timeout_buf[16];
        snprintf(timeout_buf, sizeof(timeout_buf), "%u",
                 (unsigned int)response_timeout_ms);
        const livekit_data_stream_attribute_t attrs[] = {
            { .key = LK_RPC_ATTR_REQUEST_ID,          .value = request_id },
            { .key = LK_RPC_ATTR_METHOD,              .value = options->method },
            { .key = LK_RPC_ATTR_RESPONSE_TIMEOUT_MS, .value = timeout_buf },
            { .key = LK_RPC_ATTR_VERSION,             .value = LK_RPC_VERSION_V2 },
        };
        char *destinations[1] = { (char *)options->destination_identity };
        size_t payload_len = strlen(options->payload);
        livekit_data_stream_options_t stream_opts = {
            .topic = LK_RPC_REQUEST_TOPIC,
            .is_text = true,
            .total_length = payload_len,
            .has_total_length = true,
            .attributes = attrs,
            .attributes_count = sizeof(attrs) / sizeof(attrs[0]),
            .destination_identities = destinations,
            .destination_identities_count = 1,
        };
        livekit_data_stream_handle_t stream = NULL;
        if (data_stream_writer_open(mgr->options.writer, &stream_opts, &stream)
                == DATA_STREAM_WRITER_ERR_NONE) {
            if (data_stream_writer_write(mgr->options.writer, stream,
                                         (const uint8_t *)options->payload, payload_len)
                    == DATA_STREAM_WRITER_ERR_NONE &&
                data_stream_writer_close(mgr->options.writer, stream)
                    == DATA_STREAM_WRITER_ERR_NONE) {
                sent = true;
            }
        }
    } else {
        // Build and send the v1 RpcRequest packet.
        livekit_pb_data_packet_t packet = LIVEKIT_PB_DATA_PACKET_INIT_ZERO;
        packet.which_value = LIVEKIT_PB_DATA_PACKET_RPC_REQUEST_TAG;
        strlcpy(packet.value.rpc_request.id, request_id,
                sizeof(packet.value.rpc_request.id));
        packet.value.rpc_request.method = (char *)options->method;
        packet.value.rpc_request.payload = (char *)options->payload;
        packet.value.rpc_request.response_timeout_ms = response_timeout_ms;
        packet.value.rpc_request.version = 1;

        char *destinations[1] = { (char *)options->destination_identity };
        packet.destination_identities = destinations;
        packet.destination_identities_count = 1;

        sent = mgr->options.send_packet(&packet, mgr->options.ctx);
    }

    if (sent) {
        return RPC_CLIENT_MANAGER_ERR_NONE;
    }

    // Send failed -- but a synchronous response might already have removed
    // and resolved the entry. Only fail the request if it is still pending.
    pending_request_t *still_pending = NULL;
    if (xSemaphoreTake(mgr->mutex, portMAX_DELAY) == pdTRUE) {
        still_pending = take_entry_locked(mgr, request_id);
        xSemaphoreGive(mgr->mutex);
    }
    if (still_pending != NULL) {
        deliver_result(mgr, request_id,
                       LIVEKIT_RPC_RESULT_SEND_FAILED, NULL, NULL);
        free_pending(still_pending);
        return RPC_CLIENT_MANAGER_ERR_SEND_FAILED;
    }
    return RPC_CLIENT_MANAGER_ERR_NONE;
}

static void handle_ack(rpc_client_manager_t *mgr, const livekit_pb_rpc_ack_t *ack)
{
    if (ack == NULL) {
        return;
    }
    if (xSemaphoreTake(mgr->mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    khiter_t k = kh_get(pending, mgr->pending, ack->request_id);
    if (k != kh_end(mgr->pending)) {
        pending_request_t *entry = kh_value(mgr->pending, k);
        if (!entry->ack_received) {
            entry->ack_received = true;
            if (entry->ack_timer != NULL) {
                xTimerStop(entry->ack_timer, TIMER_CMD_TICKS);
            }
        }
    } else {
        ESP_LOGD(TAG, "ack for unknown request_id %s (late?)", ack->request_id);
    }
    xSemaphoreGive(mgr->mutex);
}

static void handle_response(rpc_client_manager_t *mgr,
                            const livekit_pb_rpc_response_t *res)
{
    if (res == NULL) {
        return;
    }
    pending_request_t *entry = NULL;
    if (xSemaphoreTake(mgr->mutex, portMAX_DELAY) == pdTRUE) {
        entry = take_entry_locked(mgr, res->request_id);
        xSemaphoreGive(mgr->mutex);
    }
    if (entry == NULL) {
        ESP_LOGD(TAG, "response for unknown request_id %s (late?)", res->request_id);
        return;
    }

    if (res->which_value == LIVEKIT_PB_RPC_RESPONSE_PAYLOAD_TAG) {
        deliver_result(mgr, res->request_id, LIVEKIT_RPC_RESULT_OK,
                       res->value.payload, NULL);
    } else if (res->which_value == LIVEKIT_PB_RPC_RESPONSE_ERROR_TAG) {
        deliver_result(mgr, res->request_id,
                       (livekit_rpc_result_code_t)res->value.error.code,
                       NULL, res->value.error.data);
    } else {
        deliver_result(mgr, res->request_id,
                       LIVEKIT_RPC_RESULT_APPLICATION, NULL,
                       "malformed RPC response");
    }
    free_pending(entry);
}

void rpc_client_manager_handle_packet(rpc_client_manager_handle_t handle,
                                      const livekit_pb_data_packet_t *packet)
{
    if (handle == NULL || packet == NULL) {
        return;
    }
    rpc_client_manager_t *mgr = (rpc_client_manager_t *)handle;
    switch (packet->which_value) {
        case LIVEKIT_PB_DATA_PACKET_RPC_ACK_TAG:
            handle_ack(mgr, &packet->value.rpc_ack);
            break;
        case LIVEKIT_PB_DATA_PACKET_RPC_RESPONSE_TAG:
            handle_response(mgr, &packet->value.rpc_response);
            break;
        default:
            break;
    }
}

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

/// Linear-scan the pending map for an entry whose bound response stream
/// id matches @p stream_id. Caller must hold the mutex.
static pending_request_t *find_pending_by_stream_id_locked(rpc_client_manager_t *mgr,
                                                           const char *stream_id)
{
    if (stream_id == NULL || stream_id[0] == '\0') {
        return NULL;
    }
    for (khiter_t k = kh_begin(mgr->pending); k != kh_end(mgr->pending); ++k) {
        if (!kh_exist(mgr->pending, k)) {
            continue;
        }
        pending_request_t *entry = kh_value(mgr->pending, k);
        if (entry->response_stream_id[0] != '\0' &&
            strcmp(entry->response_stream_id, stream_id) == 0) {
            return entry;
        }
    }
    return NULL;
}

/// Take ownership of the pending entry whose bound response stream id
/// matches @p stream_id, removing it from the map.
static pending_request_t *take_pending_by_stream_id_locked(rpc_client_manager_t *mgr,
                                                           const char *stream_id)
{
    if (stream_id == NULL || stream_id[0] == '\0') {
        return NULL;
    }
    for (khiter_t k = kh_begin(mgr->pending); k != kh_end(mgr->pending); ++k) {
        if (!kh_exist(mgr->pending, k)) {
            continue;
        }
        pending_request_t *entry = kh_value(mgr->pending, k);
        if (entry->response_stream_id[0] != '\0' &&
            strcmp(entry->response_stream_id, stream_id) == 0) {
            const char *key = kh_key(mgr->pending, k);
            kh_del(pending, mgr->pending, k);
            free((void *)key);
            return entry;
        }
    }
    return NULL;
}

static bool append_response_bytes(pending_request_t *entry,
                                  const uint8_t *data, size_t size)
{
    if (size == 0) {
        return true;
    }
    size_t needed = entry->response_buf_size + size;
    if (needed > entry->response_buf_cap) {
        size_t new_cap = entry->response_buf_cap == 0 ? 256 : entry->response_buf_cap * 2;
        while (new_cap < needed) {
            new_cap *= 2;
        }
        uint8_t *grown = realloc(entry->response_buf, new_cap);
        if (grown == NULL) {
            return false;
        }
        entry->response_buf = grown;
        entry->response_buf_cap = new_cap;
    }
    memcpy(entry->response_buf + entry->response_buf_size, data, size);
    entry->response_buf_size += size;
    return true;
}

void rpc_client_manager_on_response_stream_open(rpc_client_manager_handle_t handle,
                                                const livekit_data_stream_header_t *header)
{
    if (handle == NULL || header == NULL) {
        return;
    }
    rpc_client_manager_t *mgr = (rpc_client_manager_t *)handle;
    const char *request_id = find_attribute_value(header, LK_RPC_ATTR_REQUEST_ID);
    if (request_id == NULL) {
        ESP_LOGD(TAG, "response stream %s missing %s attribute",
                 header->stream_id, LK_RPC_ATTR_REQUEST_ID);
        return;
    }
    if (xSemaphoreTake(mgr->mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    khiter_t k = kh_get(pending, mgr->pending, request_id);
    if (k == kh_end(mgr->pending)) {
        xSemaphoreGive(mgr->mutex);
        ESP_LOGD(TAG, "response stream %s for unknown request %s",
                 header->stream_id, request_id);
        return;
    }
    pending_request_t *entry = kh_value(mgr->pending, k);
    // Enforce spec test 14: sender identity must match the request's
    // destination identity. A mismatched stream is ignored: we do NOT
    // bind it, so subsequent chunks for that stream id will look up
    // nothing and be dropped silently.
    if (entry->destination_identity == NULL ||
        header->sender_identity == NULL ||
        strcmp(entry->destination_identity, header->sender_identity) != 0) {
        xSemaphoreGive(mgr->mutex);
        ESP_LOGW(TAG, "ignoring response stream for request %s from unexpected sender %s",
                 request_id,
                 header->sender_identity ? header->sender_identity : "(null)");
        return;
    }
    if (!entry->is_v2) {
        xSemaphoreGive(mgr->mutex);
        ESP_LOGW(TAG, "ignoring v2 response stream for v1 request %s", request_id);
        return;
    }
    strlcpy(entry->response_stream_id, header->stream_id, sizeof(entry->response_stream_id));
    // Header arrival implies the handler received the request, so the
    // ack timer no longer needs to fire even if the actual RpcAck packet
    // got lost or hasn't been processed yet.
    entry->ack_received = true;
    if (entry->ack_timer != NULL) {
        xTimerStop(entry->ack_timer, TIMER_CMD_TICKS);
    }
    xSemaphoreGive(mgr->mutex);
}

void rpc_client_manager_on_response_stream_chunk(rpc_client_manager_handle_t handle,
                                                 const livekit_data_stream_chunk_t *chunk)
{
    if (handle == NULL || chunk == NULL) {
        return;
    }
    rpc_client_manager_t *mgr = (rpc_client_manager_t *)handle;
    if (xSemaphoreTake(mgr->mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    pending_request_t *entry = find_pending_by_stream_id_locked(mgr, chunk->stream_id);
    if (entry != NULL && chunk->content != NULL && chunk->content_size > 0) {
        if (!append_response_bytes(entry, chunk->content, chunk->content_size)) {
            ESP_LOGE(TAG, "out of memory accumulating response for request %s",
                     entry->request_id);
        }
    }
    xSemaphoreGive(mgr->mutex);
}

void rpc_client_manager_on_response_stream_close(rpc_client_manager_handle_t handle,
                                                 const livekit_data_stream_trailer_t *trailer)
{
    if (handle == NULL || trailer == NULL) {
        return;
    }
    rpc_client_manager_t *mgr = (rpc_client_manager_t *)handle;
    pending_request_t *entry = NULL;
    if (xSemaphoreTake(mgr->mutex, portMAX_DELAY) == pdTRUE) {
        entry = take_pending_by_stream_id_locked(mgr, trailer->stream_id);
        xSemaphoreGive(mgr->mutex);
    }
    if (entry == NULL) {
        return;
    }
    // Materialize a NUL-terminated copy of the accumulated payload for
    // the on_result callback. Doing it outside the mutex avoids holding
    // the lock across the user callback.
    char *payload = malloc(entry->response_buf_size + 1);
    if (payload == NULL) {
        deliver_result(mgr, entry->request_id,
                       LIVEKIT_RPC_RESULT_APPLICATION, NULL,
                       "out of memory");
    } else {
        if (entry->response_buf_size > 0 && entry->response_buf != NULL) {
            memcpy(payload, entry->response_buf, entry->response_buf_size);
        }
        payload[entry->response_buf_size] = '\0';
        deliver_result(mgr, entry->request_id, LIVEKIT_RPC_RESULT_OK,
                       payload, NULL);
        free(payload);
    }
    free_pending(entry);
}

void rpc_client_manager_on_participant_disconnect(rpc_client_manager_handle_t handle,
                                                  const char *identity)
{
    if (handle == NULL || identity == NULL) {
        return;
    }
    rpc_client_manager_t *mgr = (rpc_client_manager_t *)handle;

    // Collect matching entries under the mutex, then deliver/cleanup outside it.
    if (xSemaphoreTake(mgr->mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    size_t cap = kh_size(mgr->pending);
    pending_request_t **victims = NULL;
    size_t victim_count = 0;
    if (cap > 0) {
        victims = malloc(cap * sizeof(*victims));
    }
    if (cap == 0 || victims != NULL) {
        for (khiter_t k = kh_begin(mgr->pending); k != kh_end(mgr->pending); ++k) {
            if (!kh_exist(mgr->pending, k)) {
                continue;
            }
            pending_request_t *entry = kh_value(mgr->pending, k);
            if (entry->destination_identity != NULL &&
                strcmp(entry->destination_identity, identity) == 0) {
                victims[victim_count++] = entry;
                const char *key = kh_key(mgr->pending, k);
                kh_del(pending, mgr->pending, k);
                free((void *)key);
            }
        }
    }
    xSemaphoreGive(mgr->mutex);

    for (size_t i = 0; i < victim_count; i++) {
        deliver_result(mgr, victims[i]->request_id,
                       LIVEKIT_RPC_RESULT_RECIPIENT_DISCONNECTED, NULL, NULL);
        free_pending(victims[i]);
    }
    free(victims);
}
