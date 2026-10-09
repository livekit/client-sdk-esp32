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

// Selects one of the two manual S31 audio implementations in this example.
// Production examples use the reusable livekit/s31_board component instead.

#include "board.h"

#include "board_function_core.h"
#include "board_korvo.h"
#include "esp_check.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "custom_s31_board";

void board_init(void)
{
#if CONFIG_LK_CUSTOM_S31_BOARD_KORVO
    ESP_LOGI(TAG, "Initializing ESP32-S31-Korvo-1 manually");
    ESP_ERROR_CHECK(s31_korvo_audio_init());
#elif CONFIG_LK_CUSTOM_S31_BOARD_FUNCTION_CORE
    ESP_LOGI(TAG, "Initializing ESP32-S31-Function-CoreBoard-1 manually");
    ESP_ERROR_CHECK(s31_function_core_audio_init());
#else
#error "Select an ESP32-S31 board profile"
#endif
}

esp_codec_dev_handle_t get_playback_handle(void)
{
#if CONFIG_LK_CUSTOM_S31_BOARD_KORVO
    return s31_korvo_audio_get_playback_handle();
#else
    return s31_function_core_audio_get_playback_handle();
#endif
}

esp_codec_dev_handle_t get_record_handle(void)
{
#if CONFIG_LK_CUSTOM_S31_BOARD_KORVO
    return s31_korvo_audio_get_record_handle();
#else
    return s31_function_core_audio_get_record_handle();
#endif
}

unsigned get_playback_channels(void)
{
#if CONFIG_LK_CUSTOM_S31_BOARD_KORVO
    return 2U;
#else
    return 1U;
#endif
}
