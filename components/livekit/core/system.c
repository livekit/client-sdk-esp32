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

#include <stdbool.h>
#include "esp_log.h"
#include "webrtc_utils_time.h"
#include "media_lib_os.h"
#include "media_lib_adapter.h"
#include "system.h"
#include "peer.h"
#include "engine.h"

#define VIDEO_ENCODE_THREAD_NAME  "venc"
#define AUDIO_ENCODE_THREAD_NAME  "aenc"
#define AUDIO_DECODE_THREAD_NAME  "Adec"
#define AEC_SRC_READ_THREAD_NAME  "SrcRead"
#define AEC_BUFFER_IN_THREAD_NAME "buffer_in"

static const char *TAG = "livekit_system";
static bool is_media_lib_setup = false;

static void thread_scheduler(const char *thread_name, media_lib_thread_cfg_t *thread_cfg)
{
    ESP_LOGD(TAG, "Scheduling thread '%s'", thread_name);

    // LiveKit threads
    if (strncmp(thread_name, PEER_THREAD_NAME_PREFIX, strlen(PEER_THREAD_NAME_PREFIX)) == 0) {
        thread_cfg->stack_size = 25 * 1024;
        thread_cfg->priority = 18;
        thread_cfg->core_id = 1;
        return;
    }
    if (strcmp(thread_name, STREAM_THREAD_NAME) == 0) {
        thread_cfg->stack_size = 4 * 1024;
        thread_cfg->priority = 15;
        thread_cfg->core_id = 1;
        return;
    }

    // Media lib threads
    if (strcmp(thread_name, AUDIO_DECODE_THREAD_NAME) == 0) {
        thread_cfg->stack_size = 40 * 1024;
        thread_cfg->priority = 10;
        thread_cfg->core_id = 1;
        return;
    }
    if (strcmp(thread_name, AUDIO_ENCODE_THREAD_NAME) == 0) {
        // Required for Opus
        thread_cfg->stack_size = 40 * 1024;
        thread_cfg->priority = 10;
        return;
    }
    if (strcmp(thread_name, AEC_SRC_READ_THREAD_NAME) == 0) {
        thread_cfg->stack_size = 40 * 1024;
        thread_cfg->priority = 16;
        thread_cfg->core_id = 0;
        return;
    }
    if (strcmp(thread_name, AEC_BUFFER_IN_THREAD_NAME) == 0) {
        thread_cfg->stack_size = 6 * 1024;
        thread_cfg->priority = 10;
        thread_cfg->core_id = 0;
        return;
    }
    if (strcmp(thread_name, VIDEO_ENCODE_THREAD_NAME) == 0) {
#if CONFIG_IDF_TARGET_ESP32S3
        thread_cfg->stack_size = 20 * 1024;
#endif
        thread_cfg->priority = 10;
        return;
    }
}

bool system_setup_media_lib(void)
{
    esp_err_t ret = media_lib_add_default_adapter();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to setup media lib");
        return false;
    }
    media_lib_thread_set_schedule_cb(thread_scheduler);
    is_media_lib_setup = true;
    return true;
}

bool system_is_media_lib_setup(void)
{
    return is_media_lib_setup;
}

bool system_sync_time(void)
{
    esp_err_t ret = webrtc_utils_time_sync_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to sync time");
        return false;
    }
    return true;
}