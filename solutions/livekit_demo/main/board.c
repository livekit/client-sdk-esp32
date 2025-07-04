#include <stdio.h>
#include "esp_log.h"
#include "codec_init.h"
#include "codec_board.h"
#include "esp_codec_dev.h"
#include "sdkconfig.h"
#include "settings.h"
#include "driver/temperature_sensor.h"

#include "board.h"

static const char *TAG = "board";

static temperature_sensor_handle_t temp_sensor = NULL;

static void init_temp_sensor(void)
{
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
    ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_sensor));
    ESP_ERROR_CHECK(temperature_sensor_enable(temp_sensor));
}

static void init_leds(void)
{
    // TODO: Initialize LEDs
}

void board_init()
{
    ESP_LOGI(TAG, "Initializing board");
    set_codec_board_type(TEST_BOARD_NAME);
    // When using performing recording and playback at the same time,
    // reuse_dev must be set to false.
    codec_init_cfg_t cfg = {
#if CONFIG_IDF_TARGET_ESP32S3
        .in_mode = CODEC_I2S_MODE_TDM,
        .in_use_tdm = true,
#endif
        .reuse_dev = false
    };
    init_codec(&cfg);
    init_temp_sensor();
    init_leds();
}

float board_get_temp(void)
{
    float temp_out;
    ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_sensor, &temp_out));
    return temp_out;
}

void board_set_led_state(board_led_t led, bool state)
{
    ESP_LOGI(TAG, "Set LED %d to %s", led, state ? "on" : "off");
    // TODO: Set LED state
}