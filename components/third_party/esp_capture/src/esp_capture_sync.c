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

#include "esp_capture_sync.h"
#include "esp_timer.h"
#include <stdbool.h>
#include <stdlib.h>

#define ELAPSE(cur, last) (cur > last ? cur - last : cur + (0xFFFFFFFF - last))
#define CUR()             (uint32_t)(esp_timer_get_time() / 1000)

typedef struct {
    esp_capture_sync_mode_t mode;
    uint32_t                last_update_time;
    uint32_t                last_update_pts;
    uint32_t                last_audio_pts;
    bool                    started;
} sync_t;

int esp_capture_sync_create(esp_capture_sync_mode_t mode, esp_capture_sync_handle_t *handle)
{
    sync_t *sync = (sync_t *)calloc(1, sizeof(sync_t));
    if (sync == NULL) {
        return ESP_CAPTURE_ERR_NO_MEM;
    }
    sync->mode = mode;
    *handle = sync;
    return ESP_CAPTURE_ERR_OK;
}

int esp_capture_sync_audio_update(esp_capture_sync_handle_t handle, uint32_t aud_pts)
{
    sync_t *sync = (sync_t *)handle;
    if (sync->mode == ESP_CAPTURE_SYNC_MODE_AUDIO) {
        sync->last_update_time = CUR();
        sync->last_update_pts = sync->last_audio_pts = aud_pts;
    }
    return ESP_CAPTURE_ERR_OK;
}

int esp_capture_sync_start(esp_capture_sync_handle_t handle)
{
    if (handle == NULL) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    sync_t *sync = (sync_t *)handle;
    sync->started = true;
    sync->last_update_time = CUR();
    return ESP_CAPTURE_ERR_OK;
}

int esp_capture_sync_stop(esp_capture_sync_handle_t handle)
{
    if (handle == NULL) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    sync_t *sync = (sync_t *)handle;
    sync->started = false;
    return ESP_CAPTURE_ERR_OK;
}

int esp_capture_sync_get_current(esp_capture_sync_handle_t handle, uint32_t *pts)
{
    if (handle == NULL) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    sync_t *sync = (sync_t *)handle;
    if (sync->started == false) {
        *pts = sync->last_update_pts;
        return ESP_CAPTURE_ERR_OK;
    }
    uint32_t cur = CUR();
    uint32_t elapse = ELAPSE(cur, sync->last_update_time);
    *pts = sync->last_update_pts + elapse;
    return ESP_CAPTURE_ERR_OK;
}

int esp_capture_sync_destroy(esp_capture_sync_handle_t handle)
{
    if (handle == NULL) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    free(handle);
    return ESP_CAPTURE_ERR_OK;
}
