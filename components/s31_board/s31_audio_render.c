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

#include <stdatomic.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "livekit_s31_audio_render.h"

static const char *TAG = "s31_audio_render";

// Decouple decoder bursts from blocking I2S writes. Reserve enough PCM to
// re-prime after DTX silence across the observed ~100 ms receive bursts.
// This implementation owns its synchronization and shutdown protocol.
#define RENDER_BUFFER_BYTES  (64U * 1024U)
#define RENDER_DRAIN_BYTES   1024U
#define RENDER_PREFILL_BYTES 38400U // 200 ms at 48 kHz, 16-bit stereo
#define RENDER_PREFILL_MS    200U
#define RENDER_WRITE_WAIT_MS 2000U
#define RENDER_FRAME_BYTES   4U
#define RENDER_BYTES_PER_MS  192U

typedef struct {
    esp_codec_dev_handle_t device;
    int volume;
} render_config_t;

typedef struct {
    render_config_t config;
    RingbufHandle_t ring;
    SemaphoreHandle_t producer_lock;
    SemaphoreHandle_t stopped;
    bool open; // protected by producer_lock
    atomic_bool stopping;
    atomic_bool failed;
    atomic_bool eos;
    atomic_uint in_flight;
} buffered_render_t;

static size_t queued_bytes(buffered_render_t *render)
{
    UBaseType_t waiting = 0;
    vRingbufferGetInfo(render->ring, NULL, NULL, NULL, NULL, &waiting);
    return waiting;
}

static void discard_queued(buffered_render_t *render)
{
    size_t size;
    void *data;
    while ((data = xRingbufferReceiveUpTo(render->ring, &size, 0,
                                         RENDER_DRAIN_BYTES)) != NULL) {
        vRingbufferReturnItem(render->ring, data);
    }
}

static void drain_task(void *arg)
{
    buffered_render_t *render = arg;
    bool started = false;
    bool filling = false;
    TickType_t fill_start = 0;
    size_t fill_waiting = 0;

    while (!atomic_load(&render->stopping) && !atomic_load(&render->failed)) {
        if (!started) {
            size_t waiting = queued_bytes(render);
            if (waiting == 0) {
                filling = false;
                fill_waiting = 0;
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            if (!filling || waiting != fill_waiting) {
                fill_start = xTaskGetTickCount();
                filling = true;
                fill_waiting = waiting;
            }
            // An idle deadline and EOS release short clips and final tails.
            // Extend the deadline while data is arriving: a sparse DTX frame
            // must not spend the next talkspurt's prefill budget in advance.
            if (waiting < RENDER_PREFILL_BYTES && !atomic_load(&render->eos) &&
                xTaskGetTickCount() - fill_start < pdMS_TO_TICKS(RENDER_PREFILL_MS)) {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            started = true;
            filling = false;
        }

        size_t bytes = 0;
        void *data = xRingbufferReceiveUpTo(render->ring, &bytes,
                                           0, RENDER_DRAIN_BYTES);
        if (data == NULL) {
            // Re-prime as soon as the PCM queue is exhausted. Waiting here
            // allowed sparse DTX packets and resumed talkspurts to bypass
            // prefill when they arrived within that wait interval. This is
            // a software queue condition, not a measured DMA underrun.
            started = false;
            continue;
        }
        atomic_store(&render->in_flight, (unsigned)bytes);
        int ret = ESP_CODEC_DEV_OK;
        if (!atomic_load(&render->stopping)) {
            ret = esp_codec_dev_write(render->config.device, data, (int)bytes);
        }
        vRingbufferReturnItem(render->ring, data);
        atomic_store(&render->in_flight, 0);
        if (ret != ESP_CODEC_DEV_OK) {
            ESP_LOGE(TAG, "Codec write failed: %d", ret);
            atomic_store(&render->failed, true);
            break;
        }
    }
    // No render state is accessed after this acknowledgement. Close can now
    // discard queued data and close the codec without racing a codec write.
    xSemaphoreGive(render->stopped);
    vTaskDelete(NULL);
}

static audio_render_handle_t buffered_init(void *cfg, int size)
{
    if (cfg == NULL || size != sizeof(render_config_t)) {
        return NULL;
    }
    render_config_t *config = cfg;
    if (config->device == NULL || config->volume < 0 || config->volume > 100) {
        return NULL;
    }
    buffered_render_t *render = calloc(1, sizeof(*render));
    if (render == NULL) {
        return NULL;
    }
    render->config = *config;
    atomic_init(&render->stopping, true);
    atomic_init(&render->failed, false);
    atomic_init(&render->eos, false);
    atomic_init(&render->in_flight, 0);
    render->ring = xRingbufferCreate(RENDER_BUFFER_BYTES, RINGBUF_TYPE_BYTEBUF);
    render->producer_lock = xSemaphoreCreateMutex();
    render->stopped = xSemaphoreCreateBinary();
    if (render->ring == NULL || render->producer_lock == NULL || render->stopped == NULL) {
        if (render->ring) vRingbufferDelete(render->ring);
        if (render->producer_lock) vSemaphoreDelete(render->producer_lock);
        if (render->stopped) vSemaphoreDelete(render->stopped);
        free(render);
        return NULL;
    }
    return render;
}

static int buffered_open(audio_render_handle_t handle, av_render_audio_frame_info_t *info)
{
    buffered_render_t *render = handle;
    if (render == NULL || info == NULL || info->sample_rate != 48000 ||
        info->channel != 2 || info->bits_per_sample != 16) {
        return -1;
    }
    xSemaphoreTake(render->producer_lock, portMAX_DELAY);
    int ret = -1;
    if (render->open) goto done;
    esp_codec_dev_sample_info_t format = {
        .sample_rate = 48000, .channel = 2, .bits_per_sample = 16,
    };
    ret = esp_codec_dev_open(render->config.device, &format);
    if (ret != ESP_CODEC_DEV_OK) goto done;
    ret = esp_codec_dev_set_out_vol(render->config.device, render->config.volume);
    if (ret != ESP_CODEC_DEV_OK) {
        esp_codec_dev_close(render->config.device);
        goto done;
    }
    atomic_store(&render->stopping, false);
    atomic_store(&render->failed, false);
    atomic_store(&render->eos, false);
    if (xTaskCreate(drain_task, "s31_audio_out", 4096, render, 6, NULL) != pdPASS) {
        atomic_store(&render->stopping, true);
        esp_codec_dev_close(render->config.device);
        ret = -1;
        goto done;
    }
    render->open = true;
    ESP_LOGI(TAG, "Open 48000 Hz, 16-bit stereo I2S, volume %d", render->config.volume);
done:
    xSemaphoreGive(render->producer_lock);
    return ret;
}

static int buffered_write(audio_render_handle_t handle, av_render_audio_frame_t *frame)
{
    buffered_render_t *render = handle;
    if (render == NULL || frame == NULL || frame->size < 0 ||
        (frame->size > 0 && frame->data == NULL) ||
        (unsigned)frame->size % RENDER_FRAME_BYTES != 0) {
        return -1;
    }
    xSemaphoreTake(render->producer_lock, portMAX_DELAY);
    int ret = -1;
    if (!render->open || atomic_load(&render->stopping) ||
        atomic_load(&render->failed) || atomic_load(&render->eos)) goto done;

    TickType_t start = xTaskGetTickCount();
    size_t offset = 0;
    while (offset < (size_t)frame->size) {
        if (atomic_load(&render->stopping) || atomic_load(&render->failed)) goto done;
        if (xTaskGetTickCount() - start >= pdMS_TO_TICKS(RENDER_WRITE_WAIT_MS)) {
            ESP_LOGE(TAG, "PCM queue write timed out");
            atomic_store(&render->failed, true);
            goto done;
        }
        size_t bytes = (size_t)frame->size - offset;
        if (bytes > RENDER_DRAIN_BYTES) bytes = RENDER_DRAIN_BYTES;
        // Chunking accepts frames larger than the ring; bounded waits allow
        // close to cancel backpressure instead of deadlocking on a full ring.
        if (xRingbufferSend(render->ring, frame->data + offset, bytes,
                            pdMS_TO_TICKS(10)) == pdTRUE) {
            offset += bytes;
        }
    }
    if (frame->eos) atomic_store(&render->eos, true);
    ret = 0;
done:
    xSemaphoreGive(render->producer_lock);
    return ret;
}

static int buffered_get_latency(audio_render_handle_t handle, uint32_t *latency)
{
    if (handle == NULL || latency == NULL) return -1;
    buffered_render_t *render = handle;
    // Software estimate only: codec/DMA latency is not exposed by this API.
    *latency = (uint32_t)(queued_bytes(render) + atomic_load(&render->in_flight)) /
               RENDER_BYTES_PER_MS;
    return 0;
}

static int buffered_get_frame_info(audio_render_handle_t handle,
                                   av_render_audio_frame_info_t *info)
{
    if (handle == NULL || info == NULL) return -1;
    *info = (av_render_audio_frame_info_t) {
        .sample_rate = 48000, .channel = 2, .bits_per_sample = 16,
    };
    return 0;
}

static int buffered_set_speed(audio_render_handle_t handle, float speed)
{
    return handle != NULL && speed == 1.0f ? 0 : -1;
}

static int buffered_close(audio_render_handle_t handle)
{
    buffered_render_t *render = handle;
    if (render == NULL) return -1;
    atomic_store(&render->stopping, true);
    xSemaphoreTake(render->producer_lock, portMAX_DELAY);
    int ret = 0;
    if (render->open) {
        // Await actual completion, never force-delete a task holding PCM or
        // touching I2S. This relies on the codec driver's write returning.
        xSemaphoreTake(render->stopped, portMAX_DELAY);
        discard_queued(render);
        ret = esp_codec_dev_close(render->config.device);
        render->open = false;
    }
    xSemaphoreGive(render->producer_lock);
    return ret;
}

static void buffered_deinit(audio_render_handle_t handle)
{
    buffered_render_t *render = handle;
    if (render == NULL) return;
    buffered_close(render);
    vRingbufferDelete(render->ring);
    vSemaphoreDelete(render->producer_lock);
    vSemaphoreDelete(render->stopped);
    free(render);
}

audio_render_handle_t livekit_s31_audio_render_alloc(
    esp_codec_dev_handle_t playback_handle, int volume)
{
    render_config_t config = { .device = playback_handle, .volume = volume };
    audio_render_cfg_t cfg = {
        .ops = {
            .init = buffered_init, .open = buffered_open, .write = buffered_write,
            .get_latency = buffered_get_latency, .get_frame_info = buffered_get_frame_info,
            .set_speed = buffered_set_speed, .close = buffered_close, .deinit = buffered_deinit,
        },
        .cfg = &config,
        .cfg_size = sizeof(config),
    };
    return audio_render_alloc_handle(&cfg);
}
