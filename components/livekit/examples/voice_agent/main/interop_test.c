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

// Optional room diagnostics for the host interoperability harness. Keep these
// endpoints separate from normal agent controls so ordinary firmware does not
// expose memory statistics or disruptive Wi-Fi cycling unless explicitly enabled.

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

#if CONFIG_LK_EXAMPLE_USE_WIFI
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

#include "interop_test.h"

#define INTEROP_STREAM_BUFFER_SIZE 1024

static const char *TAG = "interop_test";

static const char *RELIABLE_TOPIC = "lk.test.packet.reliable";
static const char *LOSSY_TOPIC = "lk.test.packet.lossy";
static const char *TEXT_TOPIC = "lk.test.stream.text";
static const char *BYTE_TOPIC = "lk.test.stream.bytes";
static const char *STATS_RPC_METHOD = "lk.test.get_system_stats";
#if CONFIG_LK_EXAMPLE_USE_WIFI
static const char *WIFI_CYCLE_RPC_METHOD = "lk.test.cycle_wifi";

#define WIFI_CYCLE_DEFAULT_MS     5000
#define WIFI_CYCLE_MIN_MS         1000
#define WIFI_CYCLE_MAX_MS         30000
#define WIFI_CYCLE_START_DELAY_MS 500

static bool wifi_cycle_in_progress;
#endif

typedef struct {
    const char *topic;
    const char *echo_topic;
    bool is_text;
    uint8_t buffer[INTEROP_STREAM_BUFFER_SIZE];
    size_t size;
    bool overflow;
} stream_echo_t;

static livekit_room_handle_t room_handle;
static stream_echo_t text_echo = {
    .topic = "lk.test.stream.text",
    .echo_topic = "lk.test.stream.text.echo",
    .is_text = true,
};
static stream_echo_t byte_echo = {
    .topic = "lk.test.stream.bytes",
    .echo_topic = "lk.test.stream.bytes.echo",
    .is_text = false,
};

static void get_system_stats(const livekit_rpc_invocation_t *invocation, void *ctx)
{
    (void)invocation;
    (void)ctx;

    char response[256];
    int length = snprintf(
        response,
        sizeof(response),
        "{\"uptime_ms\":%" PRIu64
        ",\"internal_free\":%u,\"internal_min\":%u,\"internal_largest\":%u"
        ",\"psram_free\":%u,\"psram_min\":%u,\"psram_largest\":%u}",
        (uint64_t)(esp_timer_get_time() / 1000),
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
    if (length < 0 || length >= (int)sizeof(response)) {
        livekit_rpc_return_error("Failed to format system statistics");
        return;
    }
    livekit_rpc_return_ok(response);
}

#if CONFIG_LK_EXAMPLE_USE_WIFI
static void wifi_cycle_task(void *arg)
{
    uint32_t outage_ms = (uint32_t)(uintptr_t)arg;
    // Give the scheduling RPC response time to leave before taking Wi-Fi down.
    vTaskDelay(pdMS_TO_TICKS(WIFI_CYCLE_START_DELAY_MS));

    ESP_LOGI(TAG, "Stopping WiFi for %" PRIu32 " ms", outage_ms);
    esp_err_t err = esp_wifi_stop();
    if (err == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(outage_ms));
        err = esp_wifi_start();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi recovery test failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "WiFi restarted; waiting for association and room recovery");
    }
    wifi_cycle_in_progress = false;
    vTaskDelete(NULL);
}

static void cycle_wifi(const livekit_rpc_invocation_t *invocation, void *ctx)
{
    (void)ctx;
    uint32_t outage_ms = WIFI_CYCLE_DEFAULT_MS;
    if (invocation->payload != NULL && invocation->payload[0] != '\0') {
        char *end = NULL;
        unsigned long parsed = strtoul(invocation->payload, &end, 10);
        if (end == invocation->payload || *end != '\0' ||
            parsed < WIFI_CYCLE_MIN_MS || parsed > WIFI_CYCLE_MAX_MS) {
            livekit_rpc_return_error("Outage must be 1000-30000 milliseconds");
            return;
        }
        outage_ms = (uint32_t)parsed;
    }
    if (wifi_cycle_in_progress) {
        livekit_rpc_return_error("A WiFi recovery test is already running");
        return;
    }

    wifi_cycle_in_progress = true;
    BaseType_t created = xTaskCreate(
        wifi_cycle_task,
        "s31_wifi_cycle",
        3072,
        (void *)(uintptr_t)outage_ms,
        5,
        NULL);
    if (created != pdPASS) {
        wifi_cycle_in_progress = false;
        livekit_rpc_return_error("Failed to start WiFi recovery test");
        return;
    }
    livekit_rpc_return_ok("scheduled");
}
#endif

static void on_stream_open(const livekit_data_stream_header_t *header, void *ctx)
{
    stream_echo_t *echo = (stream_echo_t *)ctx;
    echo->size = 0;
    echo->overflow = false;
    ESP_LOGI(TAG, "Opened %s stream from %s", echo->topic,
             header->sender_identity != NULL ? header->sender_identity : "unknown");
}

static void on_stream_recv(const livekit_data_stream_chunk_t *chunk, void *ctx)
{
    stream_echo_t *echo = (stream_echo_t *)ctx;
    if (chunk == NULL || (chunk->content == NULL && chunk->content_size != 0)) {
        echo->overflow = true;
        ESP_LOGE(TAG, "Received an invalid %s stream chunk", echo->topic);
        return;
    }
    // Bound diagnostic memory use and reject the whole echo on overflow so
    // the host cannot mistake a truncated stream for a successful round trip.
    if (chunk->content_size > sizeof(echo->buffer) - echo->size) {
        echo->overflow = true;
        ESP_LOGE(TAG, "%s exceeded the %u-byte test buffer", echo->topic,
                 (unsigned)sizeof(echo->buffer));
        return;
    }
    memcpy(echo->buffer + echo->size, chunk->content, chunk->content_size);
    echo->size += chunk->content_size;
}

static void on_stream_close(const livekit_data_stream_trailer_t *trailer, void *ctx)
{
    stream_echo_t *echo = (stream_echo_t *)ctx;
    if (trailer->reason[0] != '\0' || echo->overflow || room_handle == NULL) {
        ESP_LOGE(TAG, "Cannot echo %s stream: reason='%s', overflow=%d",
                 echo->topic, trailer->reason, echo->overflow);
        return;
    }

    livekit_data_stream_options_t options = {
        .topic = echo->echo_topic,
        .is_text = echo->is_text,
        .total_length = echo->size,
        .has_total_length = true,
    };
    livekit_data_stream_handle_t stream = NULL;
    livekit_err_t err = livekit_room_data_stream_open(room_handle, &options, &stream);
    if (err == LIVEKIT_ERR_NONE) {
        err = livekit_room_data_stream_write(room_handle, stream, echo->buffer, echo->size);
    }
    if (err == LIVEKIT_ERR_NONE) {
        err = livekit_room_data_stream_close(room_handle, stream);
    } else if (stream != NULL) {
        livekit_room_data_stream_close(room_handle, stream);
    }
    if (err != LIVEKIT_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to echo %s stream: %d", echo->topic, err);
    } else {
        ESP_LOGI(TAG, "Echoed %u bytes on %s", (unsigned)echo->size, echo->echo_topic);
    }
}

void interop_test_on_data_received(const livekit_data_received_t *data, void *ctx)
{
    (void)ctx;
    if (data == NULL || data->topic == NULL || data->sender_identity == NULL || room_handle == NULL) {
        return;
    }

    bool lossy;
    const char *echo_topic;
    if (strcmp(data->topic, RELIABLE_TOPIC) == 0) {
        lossy = false;
        echo_topic = "lk.test.packet.reliable.echo";
    } else if (strcmp(data->topic, LOSSY_TOPIC) == 0) {
        lossy = true;
        echo_topic = "lk.test.packet.lossy.echo";
    } else {
        return;
    }

    // Reply only to the sender and on a distinct topic to avoid echo loops
    // when several test participants share the room.
    char *destination = data->sender_identity;
    livekit_data_publish_options_t options = {
        .payload = (livekit_data_payload_t *)&data->payload,
        .topic = (char *)echo_topic,
        .lossy = lossy,
        .destination_identities = &destination,
        .destination_identities_count = 1,
    };
    livekit_err_t err = livekit_room_publish_data(room_handle, &options);
    ESP_LOGI(TAG, "Packet echo topic=%s bytes=%u result=%d", echo_topic,
             (unsigned)data->payload.size, err);
}

livekit_err_t interop_test_start(livekit_room_handle_t room)
{
    room_handle = room;
    livekit_err_t err = livekit_room_rpc_register(room, STATS_RPC_METHOD, get_system_stats);
    if (err != LIVEKIT_ERR_NONE) {
        room_handle = NULL;
        return err;
    }

    // Unwind earlier registrations on failure so a retry starts without a
    // partially enabled diagnostic service.
#if CONFIG_LK_EXAMPLE_USE_WIFI
    err = livekit_room_rpc_register(room, WIFI_CYCLE_RPC_METHOD, cycle_wifi);
    if (err != LIVEKIT_ERR_NONE) {
        livekit_room_rpc_unregister(room, STATS_RPC_METHOD);
        room_handle = NULL;
        return err;
    }
#endif

    livekit_data_stream_handler_t text_handler = {
        .on_recv = on_stream_recv,
        .on_open = on_stream_open,
        .on_close = on_stream_close,
        .ctx = &text_echo,
    };
    err = livekit_room_data_stream_topic_register(room, TEXT_TOPIC, &text_handler);
    if (err != LIVEKIT_ERR_NONE) {
#if CONFIG_LK_EXAMPLE_USE_WIFI
        livekit_room_rpc_unregister(room, WIFI_CYCLE_RPC_METHOD);
#endif
        livekit_room_rpc_unregister(room, STATS_RPC_METHOD);
        room_handle = NULL;
        return err;
    }

    livekit_data_stream_handler_t byte_handler = {
        .on_recv = on_stream_recv,
        .on_open = on_stream_open,
        .on_close = on_stream_close,
        .ctx = &byte_echo,
    };
    err = livekit_room_data_stream_topic_register(room, BYTE_TOPIC, &byte_handler);
    if (err != LIVEKIT_ERR_NONE) {
        livekit_room_data_stream_topic_unregister(room, TEXT_TOPIC);
#if CONFIG_LK_EXAMPLE_USE_WIFI
        livekit_room_rpc_unregister(room, WIFI_CYCLE_RPC_METHOD);
#endif
        livekit_room_rpc_unregister(room, STATS_RPC_METHOD);
        room_handle = NULL;
        return err;
    }

    ESP_LOGI(TAG, "Data packet and stream echo endpoints enabled");
    return LIVEKIT_ERR_NONE;
}

void interop_test_stop(void)
{
    if (room_handle == NULL) {
        return;
    }
    livekit_room_data_stream_topic_unregister(room_handle, TEXT_TOPIC);
    livekit_room_data_stream_topic_unregister(room_handle, BYTE_TOPIC);
#if CONFIG_LK_EXAMPLE_USE_WIFI
    livekit_room_rpc_unregister(room_handle, WIFI_CYCLE_RPC_METHOD);
#endif
    livekit_room_rpc_unregister(room_handle, STATS_RPC_METHOD);
    room_handle = NULL;
}
