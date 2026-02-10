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

#include "esp_capture.h"
#include "media_lib_os.h"
#include "media_lib_adapter.h"

#include "system.h"

// MARK: - Thread schedulers

/// Thread scheduler for `media_lib_sal`.
static void media_lib_scheduler(const char *name, media_lib_thread_cfg_t *cfg)
{
    // Thread names by components:
    // esp_capture: venc_0, aenc_0, buffer_in, AUD_SRC
    // av_render: Adec, ARender
    // livekit: lk_peer_sub, lk_peer_pub, lk_eng_stream

    if (strcmp(name, "venc_0") == 0) {
#if CONFIG_IDF_TARGET_ESP32S3
        // Large stack size required for H264 when not using a hardware encoder
        cfg->stack_size = 20 * 1024;
#endif
        cfg->priority = 10;
    } else if (strcmp(name, "aenc_0") == 0) {
        // Large stack size required for Opus
        cfg->stack_size = 40 * 1024;
        cfg->priority = 10;
        cfg->core_id = 1;
    } else if (strcmp(name, "buffer_in") == 0) {
        cfg->stack_size = 6 * 1024;
        cfg->priority = 10;
        cfg->core_id = 0;
    } else if (strcmp(name, "AUD_SRC") == 0) {
        cfg->stack_size = 40 * 1024;
        cfg->priority = 15;
    } else if (strcmp(name, "lk_peer_sub") == 0 || strcmp(name, "lk_peer_pub") == 0) {
        cfg->stack_size = 25 * 1024;
        cfg->priority = 18;
        cfg->core_id = 1;
    } else if (strcmp(name, "lk_eng_stream") == 0) {
        cfg->stack_size = 4 * 1024;
        cfg->priority = 15;
        cfg->core_id = 1;
    } else if (strcmp(name, "Adec") == 0) {
        cfg->stack_size = 40 * 1024;
        cfg->priority = 15;
        cfg->core_id = 0;
    } else if (strcmp(name, "ARender") == 0) {
        cfg->priority = 20;
    }
}

/// Thread scheduler for `esp_capture`.
static void capture_scheduler(const char *name, esp_capture_thread_schedule_cfg_t *cfg)
{
    media_lib_thread_cfg_t media_lib_cfg = {
        .stack_size = cfg->stack_size,
        .priority = cfg->priority,
        .core_id = cfg->core_id,
    };
    media_lib_scheduler(name, &media_lib_cfg);

    cfg->stack_in_ext = true;
    cfg->stack_size = media_lib_cfg.stack_size;
    cfg->priority = media_lib_cfg.priority;
    cfg->core_id = (uint8_t)(media_lib_cfg.core_id & 0x0F);
}

// MARK: - Public API

static bool init_performed = false;

esp_err_t system_init(void)
{
    if (init_performed) {
        return ESP_OK;
    }
    esp_err_t ret = media_lib_add_default_adapter();
    if (ret != ESP_OK) return ret;

    ret = esp_capture_set_thread_scheduler(capture_scheduler);
    if (ret != ESP_OK) return ret;

    media_lib_thread_set_schedule_cb(media_lib_scheduler);

    init_performed = true;
    return ESP_OK;
}

bool system_init_is_done(void)
{
    return init_performed;
}
