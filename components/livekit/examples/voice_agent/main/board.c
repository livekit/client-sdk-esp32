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

#include "driver/temperature_sensor.h"
#include "esp_log.h"
#if CONFIG_IDF_TARGET_ESP32S31
#include "livekit_s31_board.h"
#else
#include "codec_board.h"
#include "codec_init.h"
#endif
#if CONFIG_IDF_TARGET_ESP32S3
#include "bsp/esp-bsp.h"
#endif

#include "board.h"

static const char *TAG = "board";
static temperature_sensor_handle_t temp_sensor;

void board_init(void)
{
    ESP_LOGI(TAG, "Initializing board");

#if CONFIG_IDF_TARGET_ESP32S3
    bsp_i2c_init();
    bsp_leds_init();
#endif

    temperature_sensor_config_t temp_sensor_config =
        TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
    ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_sensor));
    ESP_ERROR_CHECK(temperature_sensor_enable(temp_sensor));

#if CONFIG_IDF_TARGET_ESP32S31
    esp_err_t codec_ret =
        livekit_s31_board_init(CONFIG_LK_EXAMPLE_CODEC_BOARD_TYPE);
    if (codec_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize S31 audio board, ret=%s",
                 esp_err_to_name(codec_ret));
        return;
    }
#else
    set_codec_board_type(CONFIG_LK_EXAMPLE_CODEC_BOARD_TYPE);
    codec_init_cfg_t cfg = {
        .in_mode = CODEC_I2S_MODE_TDM,
        .in_use_tdm = true,
        .reuse_dev = false,
    };
    int codec_ret = init_codec(&cfg);
    if (codec_ret != 0) {
        ESP_LOGE(TAG, "Failed to initialize codec board, ret=%d", codec_ret);
    }
#endif
}

esp_codec_dev_handle_t board_get_playback_handle(void)
{
#if CONFIG_IDF_TARGET_ESP32S31
    return livekit_s31_board_get_playback_handle();
#else
    return get_playback_handle();
#endif
}

esp_codec_dev_handle_t board_get_record_handle(void)
{
#if CONFIG_IDF_TARGET_ESP32S31
    return livekit_s31_board_get_record_handle();
#else
    return get_record_handle();
#endif
}

unsigned board_get_playback_channels(void)
{
#if CONFIG_IDF_TARGET_ESP32S31
    return livekit_s31_board_get_playback_channels();
#else
    return 2U;
#endif
}

float board_get_temp(void)
{
    float temp_out;
    ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_sensor, &temp_out));
    return temp_out;
}

bool board_set_led_state(const char *color, bool state)
{
#if CONFIG_IDF_TARGET_ESP32S31
    return livekit_s31_board_set_led(color, state);
#elif CONFIG_IDF_TARGET_ESP32S3
    bsp_led_t led;
    if (strncmp(color, "red", 3) == 0) {
        // TODO: there is a bug in the Korvo2 BSP which causes the LED pins to be swapped
        // (i.e., blue is mapped to red and red is mapped to blue): https://github.com/espressif/esp-bsp/pull/632
        led = BSP_LED_BLUE;
    } else if (strncmp(color, "blue", 4) == 0) {
        led = BSP_LED_RED;
    } else {
        return false;
    }
    return bsp_led_set(led, state) == ESP_OK;
#else
    ESP_LOGW(TAG, "Indicator LED '%s' is not mapped for this board", color);
    return false;
#endif
}

const char *board_get_info_json(void)
{
#if CONFIG_IDF_TARGET_ESP32S31
    if (livekit_s31_board_get_type() == LIVEKIT_S31_BOARD_KORVO) {
        return "{\"board\":\"ESP32-S31-Korvo-1\","
               "\"audio\":\"ES8389 full-duplex capture and stereo speaker connectors\","
               "\"camera\":\"onboard OV3660; supported by the minimal_video example\","
               "\"led\":\"one addressable RGB LED; red, green, blue, or white; GPIO37\","
               "\"exposed_controls\":[\"RGB LED\",\"CPU temperature\"],"
#if CONFIG_LK_EXAMPLE_ENABLE_AEC
               "\"aec_enabled\":true,\"aec_mode\":\"VOIP_HIGH_PERF\","
#else
               "\"aec_enabled\":false,"
#endif
               "\"limitations\":[\"camera is not exposed by voice_agent\"]}";
    }
    if (livekit_s31_board_get_type() ==
        LIVEKIT_S31_BOARD_FUNCTION_COREBOARD) {
        return "{\"board\":\"ESP32-S31-Function-CoreBoard-1\","
               "\"audio\":\"ES8311 full-duplex mono onboard microphone and J9 speaker output\","
               "\"led\":\"one addressable RGB LED; red, green, blue, or white; GPIO60\","
               "\"exposed_controls\":[\"RGB LED\",\"CPU temperature\"],"
#if CONFIG_LK_EXAMPLE_ENABLE_AEC
               "\"aec_enabled\":true,\"aec_mode\":\"VOIP_HIGH_PERF\","
#else
               "\"aec_enabled\":false,"
#endif
               "\"limitations\":[]}";
    }
    return "{\"board\":\"unknown ESP32-S31 board\",\"exposed_controls\":[\"CPU temperature\"]}";
#elif CONFIG_IDF_TARGET_ESP32S3
    return "{\"board\":\"ESP32-S3-Korvo-2\","
           "\"led\":\"independent red and blue indicator LEDs\","
           "\"exposed_controls\":[\"red LED\",\"blue LED\",\"CPU temperature\"]}";
#elif CONFIG_IDF_TARGET_ESP32P4
    return "{\"board\":\"ESP32-P4 development board\","
           "\"exposed_controls\":[\"CPU temperature\"],"
           "\"limitations\":[\"indicator LED control is not exposed by this example\"]}";
#else
    return "{\"board\":\"ESP32 development board\",\"exposed_controls\":[\"CPU temperature\"]}";
#endif
}
