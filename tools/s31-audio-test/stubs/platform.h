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

#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <threads.h>

/// Millisecond clock used by the host fake.
typedef uint32_t TickType_t;
/// Unsigned counter type matching the FreeRTOS interface.
typedef unsigned UBaseType_t;
/// Status type matching the FreeRTOS interface.
typedef int BaseType_t;
/// Host mutex or binary semaphore state.
typedef struct test_semaphore *SemaphoreHandle_t;
/// Host byte-ring state with synchronized access.
typedef struct test_ring *RingbufHandle_t;
/// Opaque task handle; the fake runs detached host threads.
typedef void *TaskHandle_t;
/// Sentinel for an unbounded host wait.
#define portMAX_DELAY UINT32_MAX
/// The host fake uses one millisecond per tick.
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
/// FreeRTOS-compatible true result.
#define pdTRUE 1
/// FreeRTOS-compatible success result.
#define pdPASS 1
/// Only byte-buffer rings are supported by the fake.
#define RINGBUF_TYPE_BYTEBUF 1

/// Create a host-backed mutex.
SemaphoreHandle_t xSemaphoreCreateMutex(void);
/// Create an initially empty binary semaphore.
SemaphoreHandle_t xSemaphoreCreateBinary(void);
/// Wait for ownership or a signal up to the supplied deadline.
int xSemaphoreTake(SemaphoreHandle_t sem, TickType_t wait);
/// Release ownership or signal a waiter.
int xSemaphoreGive(SemaphoreHandle_t sem);
/// Release the fake semaphore after users have stopped.
void vSemaphoreDelete(SemaphoreHandle_t sem);
/// Allocate a bounded byte ring.
RingbufHandle_t xRingbufferCreate(size_t size, int type);
/// Release the ring after producer and consumer stop.
void vRingbufferDelete(RingbufHandle_t ring);
/// Expose queued byte count used by renderer latency logic.
void vRingbufferGetInfo(RingbufHandle_t ring, void *a, void *b, void *c, void *d, UBaseType_t *waiting);
/// Borrow a contiguous payload limited by the requested byte count.
void *xRingbufferReceiveUpTo(RingbufHandle_t ring, size_t *size, TickType_t wait, size_t maximum);
/// Copy bytes into the ring within the requested wait.
int xRingbufferSend(RingbufHandle_t ring, const void *data, size_t size, TickType_t wait);
/// Release the payload borrowed by the consumer.
void vRingbufferReturnItem(RingbufHandle_t ring, void *data);
/// Return host time in milliseconds.
TickType_t xTaskGetTickCount(void);
/// Yield the host thread for the requested milliseconds.
void vTaskDelay(TickType_t ticks);
/// Run the renderer worker on a detached host thread.
int xTaskCreate(void (*entry)(void *), const char *name, unsigned stack, void *arg, unsigned priority, TaskHandle_t *task);
/// Terminate the calling worker thread.
void vTaskDelete(TaskHandle_t task);
