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

#include <string.h>

// Select hardware once and keep board-specific codec, capture and camera
// details out of the component-based examples. Custom hardware stays separate.

#include "esp_log.h"
#include "livekit_s31_board.h"
#include "s31_function_core_audio.h"
#include "s31_korvo_audio.h"
#include "s31_korvo_camera.h"
#include "s31_rgb_led.h"

#define S31_KORVO_1_RGB_LED_GPIO 37
#define S31_FUNCTION_COREBOARD_1_RGB_LED_GPIO 60

static const char *TAG = "livekit_s31_board";
static livekit_s31_board_type_t selected_board = LIVEKIT_S31_BOARD_NONE;

// The board and example capture system are singletons. Retain the underlying
// reader while changing only the PCM slot layout, not frame size or timestamps.
static esp_capture_err_t (*microphone_read_frame)(esp_capture_audio_src_if_t *,
                                                 esp_capture_stream_frame_t *);

static esp_capture_err_t read_korvo_microphone(esp_capture_audio_src_if_t *source,
                                            esp_capture_stream_frame_t *frame)
{
    if (frame == NULL || frame->data == NULL || frame->size % 4 != 0) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    esp_capture_err_t ret = microphone_read_frame(source, frame);
    if (ret != ESP_CAPTURE_ERR_OK) {
        return ret;
    }
    // Averaging raw Korvo slots can cancel the microphone. Keep the left input
    // in both slots so esp_capture can still perform its normal mono conversion.
    uint8_t *pcm = frame->data;
    for (int i = 0; i < frame->size; i += 4) {
        pcm[i + 2] = pcm[i];
        pcm[i + 3] = pcm[i + 1];
    }
    return ESP_CAPTURE_ERR_OK;
}

esp_err_t livekit_s31_board_configure_capture(esp_capture_audio_src_if_t *source,
                                            bool use_aec)
{
    if (source == NULL || source->set_fixed_caps == NULL || source->read_frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (selected_board == LIVEKIT_S31_BOARD_NONE) {
        return ESP_ERR_INVALID_STATE;
    }
    // Capture and playback share the I2S clock. Convert for the Opus sink in
    // esp_capture rather than changing the hardware clock when tracks open.
    esp_capture_audio_info_t caps = {
        .format_id = ESP_CAPTURE_FMT_ID_PCM,
        .sample_rate = 48000,
        .channel = 2,
        .bits_per_sample = 16,
    };
    if (source->set_fixed_caps(source, &caps) != ESP_CAPTURE_ERR_OK) {
        return ESP_FAIL;
    }
    // AEC needs its separate reference slot; never duplicate that input.
    if (!use_aec && selected_board == LIVEKIT_S31_BOARD_KORVO &&
        source->read_frame != read_korvo_microphone) {
        microphone_read_frame = source->read_frame;
        source->read_frame = read_korvo_microphone;
    } else if (use_aec && source->read_frame == read_korvo_microphone) {
        source->read_frame = microphone_read_frame;
    }
    return ESP_OK;
}

esp_err_t livekit_s31_board_init(const char *board_name)
{
    if (board_name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (selected_board != LIVEKIT_S31_BOARD_NONE) {
        return ESP_OK;
    }

    esp_err_t ret;
    int rgb_gpio;
    if (strcmp(board_name, LIVEKIT_S31_BOARD_KORVO_1) == 0) {
        ret = s31_korvo_audio_init();
        selected_board = LIVEKIT_S31_BOARD_KORVO;
        rgb_gpio = S31_KORVO_1_RGB_LED_GPIO;
    } else if (strcmp(board_name,
                      LIVEKIT_S31_BOARD_FUNCTION_COREBOARD_1) == 0) {
        ret = s31_function_core_audio_init();
        selected_board = LIVEKIT_S31_BOARD_FUNCTION_COREBOARD;
        rgb_gpio = S31_FUNCTION_COREBOARD_1_RGB_LED_GPIO;
    } else {
        ESP_LOGE(TAG, "Unsupported ESP32-S31 board profile: %s", board_name);
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (ret != ESP_OK) {
        selected_board = LIVEKIT_S31_BOARD_NONE;
        return ret;
    }

    ret = s31_rgb_led_init(rgb_gpio);
    if (ret != ESP_OK) {
        // The status LED is optional; audio and LiveKit operation can continue.
        ESP_LOGW(TAG, "RGB LED is unavailable on GPIO%d: %s", rgb_gpio,
                 esp_err_to_name(ret));
    }
    ESP_LOGI(TAG, "Selected %s", livekit_s31_board_get_name());
    return ESP_OK;
}

livekit_s31_board_type_t livekit_s31_board_get_type(void)
{
    return selected_board;
}

const char *livekit_s31_board_get_name(void)
{
    switch (selected_board) {
    case LIVEKIT_S31_BOARD_KORVO:
        return "ESP32-S31-Korvo-1";
    case LIVEKIT_S31_BOARD_FUNCTION_COREBOARD:
        return "ESP32-S31-Function-CoreBoard-1";
    default:
        return "unknown ESP32-S31 board";
    }
}

esp_codec_dev_handle_t livekit_s31_board_get_playback_handle(void)
{
    switch (selected_board) {
    case LIVEKIT_S31_BOARD_KORVO:
        return s31_korvo_audio_get_playback_handle();
    case LIVEKIT_S31_BOARD_FUNCTION_COREBOARD:
        return s31_function_core_audio_get_playback_handle();
    default:
        return NULL;
    }
}

esp_codec_dev_handle_t livekit_s31_board_get_record_handle(void)
{
    switch (selected_board) {
    case LIVEKIT_S31_BOARD_KORVO:
        return s31_korvo_audio_get_record_handle();
    case LIVEKIT_S31_BOARD_FUNCTION_COREBOARD:
        return s31_function_core_audio_get_record_handle();
    default:
        return NULL;
    }
}

unsigned livekit_s31_board_get_playback_channels(void)
{
    switch (selected_board) {
    case LIVEKIT_S31_BOARD_KORVO:
        return 2U;
    case LIVEKIT_S31_BOARD_FUNCTION_COREBOARD:
        return 1U;
    default:
        return 0U;
    }
}

esp_err_t livekit_s31_board_camera_init(void)
{
    switch (selected_board) {
    case LIVEKIT_S31_BOARD_KORVO:
        return s31_korvo_camera_init();
    case LIVEKIT_S31_BOARD_FUNCTION_COREBOARD:
        ESP_LOGW(TAG, "%s has no onboard camera",
                 livekit_s31_board_get_name());
        return ESP_ERR_NOT_SUPPORTED;
    default:
        return ESP_ERR_INVALID_STATE;
    }
}

esp_err_t livekit_s31_board_camera_apply_orientation(void)
{
    switch (selected_board) {
    case LIVEKIT_S31_BOARD_KORVO:
        return s31_korvo_camera_apply_orientation();
    case LIVEKIT_S31_BOARD_FUNCTION_COREBOARD:
        return ESP_ERR_NOT_SUPPORTED;
    default:
        return ESP_ERR_INVALID_STATE;
    }
}

bool livekit_s31_board_set_led(const char *color, bool state)
{
    return s31_rgb_led_set(color, state);
}
