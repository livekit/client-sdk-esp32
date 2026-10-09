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

// Compile the actual implementation against a threaded host fake, not a
// second model of its logic. This does not emulate I2S, DMA, or acoustics.
#include <assert.h>
#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif
#include "platform.h"
#include "../../components/s31_board/s31_audio_render.c"

struct test_semaphore { mtx_t lock; unsigned count; };
struct test_ring { mtx_t lock; size_t capacity, head, tail, waiting, held; uint8_t *data; };
typedef struct { size_t size; uint8_t data[RENDER_DRAIN_BYTES]; } ring_item_t;
static atomic_bool fail_task;
static atomic_int workers;

TickType_t xTaskGetTickCount(void)
{
    struct timespec now;
    timespec_get(&now, TIME_UTC);
    return (TickType_t)((uint64_t)now.tv_sec * 1000 + (unsigned long)now.tv_nsec / 1000000);
}
void vTaskDelay(TickType_t ticks)
{
    struct timespec delay = { .tv_sec = ticks / 1000, .tv_nsec = (long)(ticks % 1000) * 1000000 };
    thrd_sleep(&delay, NULL);
}
static bool expired(TickType_t start, TickType_t wait)
{
    return wait != portMAX_DELAY && xTaskGetTickCount() - start >= wait;
}
SemaphoreHandle_t xSemaphoreCreateBinary(void)
{
    SemaphoreHandle_t sem = calloc(1, sizeof(*sem));
    assert(sem && mtx_init(&sem->lock, mtx_plain) == thrd_success);
    return sem;
}
SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    SemaphoreHandle_t sem = xSemaphoreCreateBinary();
    sem->count = 1;
    return sem;
}
int xSemaphoreTake(SemaphoreHandle_t sem, TickType_t wait)
{
    TickType_t start = xTaskGetTickCount();
    do {
        mtx_lock(&sem->lock);
        bool available = sem->count != 0;
        if (available) sem->count--;
        mtx_unlock(&sem->lock);
        if (available) return pdTRUE;
        if (expired(start, wait)) return 0;
        vTaskDelay(1);
    } while (true);
}
int xSemaphoreGive(SemaphoreHandle_t sem)
{
    mtx_lock(&sem->lock);
    assert(sem->count == 0);
    sem->count = 1;
    mtx_unlock(&sem->lock);
    return pdTRUE;
}
void vSemaphoreDelete(SemaphoreHandle_t sem) { mtx_destroy(&sem->lock); free(sem); }
RingbufHandle_t xRingbufferCreate(size_t size, int type)
{
    assert(type == RINGBUF_TYPE_BYTEBUF);
    RingbufHandle_t ring = calloc(1, sizeof(*ring));
    assert(ring && mtx_init(&ring->lock, mtx_plain) == thrd_success);
    ring->data = malloc(size);
    assert(ring->data);
    ring->capacity = size;
    return ring;
}
void vRingbufferDelete(RingbufHandle_t ring)
{
    assert(ring->held == 0);
    mtx_destroy(&ring->lock);
    free(ring->data);
    free(ring);
}
void vRingbufferGetInfo(RingbufHandle_t ring, void *a, void *b, void *c, void *d, UBaseType_t *waiting)
{
    (void)a; (void)b; (void)c; (void)d;
    mtx_lock(&ring->lock);
    *waiting = (UBaseType_t)ring->waiting;
    mtx_unlock(&ring->lock);
}
int xRingbufferSend(RingbufHandle_t ring, const void *data, size_t size, TickType_t wait)
{
    if (size > ring->capacity) return 0;
    TickType_t start = xTaskGetTickCount();
    do {
        mtx_lock(&ring->lock);
        if (ring->waiting + ring->held + size <= ring->capacity) {
            for (size_t i = 0; i < size; i++) {
                ring->data[ring->tail++ % ring->capacity] = ((const uint8_t *)data)[i];
            }
            ring->waiting += size;
            mtx_unlock(&ring->lock);
            return pdTRUE;
        }
        mtx_unlock(&ring->lock);
        if (expired(start, wait)) return 0;
        vTaskDelay(1);
    } while (true);
}
void *xRingbufferReceiveUpTo(RingbufHandle_t ring, size_t *size, TickType_t wait, size_t maximum)
{
    TickType_t start = xTaskGetTickCount();
    do {
        mtx_lock(&ring->lock);
        if (ring->waiting) {
            *size = ring->waiting < maximum ? ring->waiting : maximum;
            // Return only a contiguous region, as the real byte ring does.
            size_t contiguous = ring->capacity - ring->head % ring->capacity;
            if (*size > contiguous) *size = contiguous;
            assert(*size <= RENDER_DRAIN_BYTES);
            ring_item_t *item = malloc(sizeof(*item));
            assert(item);
            item->size = *size;
            for (size_t i = 0; i < *size; i++) item->data[i] = ring->data[ring->head++ % ring->capacity];
            ring->waiting -= *size;
            ring->held += *size;
            mtx_unlock(&ring->lock);
            return item->data;
        }
        mtx_unlock(&ring->lock);
        if (expired(start, wait)) return NULL;
        vTaskDelay(1);
    } while (true);
}
void vRingbufferReturnItem(RingbufHandle_t ring, void *data)
{
    ring_item_t *item = (ring_item_t *)((uint8_t *)data - offsetof(ring_item_t, data));
    mtx_lock(&ring->lock);
    assert(ring->held >= item->size);
    ring->held -= item->size;
    mtx_unlock(&ring->lock);
    free(item);
}
typedef struct { void (*entry)(void *); void *arg; } task_start_t;
static int start_task(void *arg)
{
    task_start_t start = *(task_start_t *)arg;
    free(arg);
    start.entry(start.arg);
    abort(); // Production task must exit through vTaskDelete.
}
int xTaskCreate(void (*entry)(void *), const char *name, unsigned stack, void *arg, unsigned priority, TaskHandle_t *task)
{
    (void)name; (void)stack; (void)priority; (void)task;
    if (atomic_load(&fail_task)) return 0;
    task_start_t *start = malloc(sizeof(*start));
    assert(start);
    *start = (task_start_t){ entry, arg };
    thrd_t thread;
    atomic_fetch_add(&workers, 1);
    assert(thrd_create(&thread, start_task, start) == thrd_success);
    thrd_detach(thread);
    return pdPASS;
}
void vTaskDelete(TaskHandle_t task)
{
    assert(task == NULL); // Never force-delete a writer with a live ring item.
    atomic_fetch_sub(&workers, 1);
    thrd_exit(0);
}

typedef struct {
    atomic_bool active, block, writing, fail_write, fail_volume;
    atomic_uint received, first_write_at, opens, closes, volumes;
    uint8_t output[262144];
} fake_codec_t;
static fake_codec_t codec;
int esp_codec_dev_open(esp_codec_dev_handle_t device, esp_codec_dev_sample_info_t *info)
{
    assert(device == &codec && info->sample_rate == 48000 && info->channel == 2);
    assert(!atomic_exchange(&codec.active, true));
    atomic_fetch_add(&codec.opens, 1);
    return 0;
}
int esp_codec_dev_close(esp_codec_dev_handle_t device)
{
    assert(device == &codec && !atomic_load(&codec.writing));
    assert(atomic_exchange(&codec.active, false));
    atomic_fetch_add(&codec.closes, 1);
    return 0;
}
int esp_codec_dev_set_out_vol(esp_codec_dev_handle_t device, int volume)
{
    assert(device == &codec && atomic_load(&codec.active) && volume == 75);
    atomic_fetch_add(&codec.volumes, 1);
    return atomic_load(&codec.fail_volume) ? -7 : 0;
}
int esp_codec_dev_write(esp_codec_dev_handle_t device, void *data, int size)
{
    assert(device == &codec && atomic_load(&codec.active));
    assert(size > 0 && size <= 1024 && size % 4 == 0);
    atomic_store(&codec.writing, true);
    while (atomic_load(&codec.block)) vTaskDelay(1);
    vTaskDelay(1);
    if (atomic_load(&codec.fail_write)) {
        atomic_store(&codec.writing, false);
        return -9;
    }
    unsigned count = atomic_load(&codec.received);
    if (count == 0) atomic_store(&codec.first_write_at, xTaskGetTickCount());
    assert(count + (unsigned)size <= sizeof(codec.output));
    memcpy(codec.output + count, data, (size_t)size);
    atomic_store(&codec.received, count + (unsigned)size);
    atomic_store(&codec.writing, false);
    return 0;
}
audio_render_handle_t audio_render_alloc_handle(audio_render_cfg_t *cfg)
{
    return cfg->ops.init(cfg->cfg, cfg->cfg_size);
}
static av_render_audio_frame_info_t format = { .sample_rate = 48000, .channel = 2, .bits_per_sample = 16 };
static uint8_t pcm[2 * RENDER_BUFFER_BYTES];
static void await_bytes(unsigned count)
{
    TickType_t start = xTaskGetTickCount();
    while (atomic_load(&codec.received) < count && !expired(start, 1500)) vTaskDelay(1);
    assert(atomic_load(&codec.received) == count);
}
static void await_writing(void)
{
    TickType_t start = xTaskGetTickCount();
    while (!atomic_load(&codec.writing) && !expired(start, 1500)) vTaskDelay(1);
    assert(atomic_load(&codec.writing));
}
static void open_render(buffered_render_t *render)
{
    atomic_store(&codec.received, 0);
    assert(buffered_open(render, &format) == 0);
}
typedef struct { buffered_render_t *render; atomic_bool done; int result; } call_t;
static int concurrent_write(void *arg)
{
    call_t *call = arg;
    av_render_audio_frame_t frame = { .data = pcm, .size = sizeof(pcm) };
    call->result = buffered_write(call->render, &frame);
    atomic_store(&call->done, true);
    return 0;
}
static int concurrent_close(void *arg)
{
    call_t *call = arg;
    call->result = buffered_close(call->render);
    atomic_store(&call->done, true);
    return 0;
}
int main(void)
{
#ifdef _MSC_VER
    // Fail unattended tests on stderr instead of opening a Windows dialog.
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    for (size_t i = 0; i < sizeof(pcm); i++) pcm[i] = (uint8_t)i;
    buffered_render_t *render = livekit_s31_audio_render_alloc(&codec, 75);
    assert(render);
    assert(livekit_s31_audio_render_alloc(NULL, 75) == NULL);
    assert(livekit_s31_audio_render_alloc(&codec, 101) == NULL);
    av_render_audio_frame_info_t invalid = format;
    invalid.sample_rate = 16000;
    assert(buffered_open(render, &invalid) != 0);
    open_render(render);
    av_render_audio_frame_t frame = { .data = pcm, .size = 1920 };
    TickType_t submitted_at = xTaskGetTickCount();
    assert(buffered_write(render, &frame) == 0);
    await_bytes(1920); // Less than prefill, no EOS: deadline must release it.
    assert(atomic_load(&codec.first_write_at) - submitted_at >= RENDER_PREFILL_MS);
    vTaskDelay(150); // Force idle/rebuffer and check another short tail.
    assert(buffered_write(render, &frame) == 0);
    await_bytes(3840);
    assert(buffered_close(render) == 0);
    puts("PASS short clips, startup prefill and idle restart");

    open_render(render);
    frame = (av_render_audio_frame_t){ .data = pcm, .size = 1920 };
    assert(buffered_write(render, &frame) == 0);
    await_bytes(1920);
    vTaskDelay(30); // Empty for less than the old 100 ms queue wait.
    assert(buffered_write(render, &frame) == 0);
    vTaskDelay(30);
    // A sparse DTX frame must not leave the next talkspurt unbuffered.
    assert(atomic_load(&codec.received) == 1920);
    await_bytes(3840);
    assert(buffered_close(render) == 0);
    puts("PASS short queue starvation re-primes before resumed playback");

    open_render(render);
    assert(buffered_write(render, &frame) == 0);
    vTaskDelay(RENDER_PREFILL_MS - 50);
    assert(buffered_write(render, &frame) == 0);
    vTaskDelay(80); // Beyond first arrival's deadline, within last arrival's.
    assert(atomic_load(&codec.received) == 0);
    await_bytes(3840);
    assert(buffered_close(render) == 0);
    puts("PASS arriving data extends short-tail prefill deadline");

    open_render(render);
    frame.eos = true;
    assert(buffered_write(render, &frame) == 0);
    await_bytes(1920);
    assert(buffered_write(render, &frame) != 0);
    assert(buffered_close(render) == 0);
    puts("PASS EOS releases prefill and rejects data after EOS");

    open_render(render);
    frame = (av_render_audio_frame_t){ .data = pcm, .size = sizeof(pcm), .eos = true };
    assert(buffered_write(render, &frame) == 0);
    await_bytes(sizeof(pcm));
    assert(memcmp(codec.output, pcm, sizeof(pcm)) == 0);
    assert(buffered_close(render) == 0);
    puts("PASS oversized frame, ring wrap and byte-exact backpressure");

    open_render(render);
    frame = (av_render_audio_frame_t){ .data = pcm, .size = -4 };
    assert(buffered_write(render, &frame) != 0);
    frame.size = 3;
    assert(buffered_write(render, &frame) != 0);
    frame.size = 4; frame.data = NULL;
    assert(buffered_write(render, &frame) != 0);
    frame.size = 0; frame.eos = true;
    assert(buffered_write(render, &frame) == 0);
    assert(buffered_close(render) == 0);
    puts("PASS invalid frames and empty EOS");

    open_render(render);
    atomic_store(&codec.block, true);
    call_t writer = { .render = render }, closer = { .render = render };
    thrd_t producer, close_thread;
    assert(thrd_create(&producer, concurrent_write, &writer) == thrd_success);
    await_writing();
    assert(thrd_create(&close_thread, concurrent_close, &closer) == thrd_success);
    TickType_t cancelled_at = xTaskGetTickCount();
    while (!atomic_load(&writer.done) && !expired(cancelled_at, 1500)) vTaskDelay(1);
    assert(atomic_load(&writer.done) && writer.result != 0);
    assert(!atomic_load(&closer.done) && atomic_load(&codec.active));
    atomic_store(&codec.block, false);
    thrd_join(producer, NULL);
    thrd_join(close_thread, NULL);
    assert(closer.result == 0 && queued_bytes(render) == 0);
    open_render(render);
    vTaskDelay(150);
    assert(atomic_load(&codec.received) == 0); // No stale PCM from old track.
    assert(buffered_close(render) == 0);
    puts("PASS close cancels blocked producer, waits for codec and clears old PCM");

    open_render(render);
    atomic_store(&codec.fail_write, true);
    frame = (av_render_audio_frame_t){ .data = pcm, .size = 1920, .eos = true };
    assert(buffered_write(render, &frame) == 0);
    TickType_t start = xTaskGetTickCount();
    while (!atomic_load(&render->failed) && !expired(start, 1500)) vTaskDelay(1);
    assert(atomic_load(&render->failed));
    assert(buffered_close(render) == 0);
    atomic_store(&codec.fail_write, false);
    atomic_store(&fail_task, true);
    assert(buffered_open(render, &format) != 0 && !atomic_load(&codec.active));
    atomic_store(&fail_task, false);
    atomic_store(&codec.fail_volume, true);
    assert(buffered_open(render, &format) != 0 && !atomic_load(&codec.active));
    atomic_store(&codec.fail_volume, false);
    puts("PASS codec error, task creation and volume failure cleanup");

    open_render(render);
    atomic_store(&codec.block, true);
    writer = (call_t){ .render = render };
    assert(thrd_create(&producer, concurrent_write, &writer) == thrd_success);
    thrd_join(producer, NULL); // Must time out rather than hang forever.
    assert(writer.result != 0 && atomic_load(&render->failed));
    atomic_store(&codec.block, false);
    assert(buffered_close(render) == 0);
    puts("PASS stalled consumer yields bounded producer timeout");

    for (unsigned i = 0; i < 20; i++) {
        open_render(render);
        assert(buffered_close(render) == 0);
    }
    assert(atomic_load(&codec.opens) == atomic_load(&codec.closes));
    assert(atomic_load(&codec.opens) == atomic_load(&codec.volumes));
    buffered_deinit(render);
    start = xTaskGetTickCount();
    while (atomic_load(&workers) && !expired(start, 1500)) vTaskDelay(1);
    assert(atomic_load(&workers) == 0);
    puts("PASS repeated open/close, volume reapply and worker cleanup");
    return 0;
}
