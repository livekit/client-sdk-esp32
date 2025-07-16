#include "esp_log.h"
#include "codec_init.h"
#include "codec_board.h"
#include "driver/temperature_sensor.h"
#include "bsp/esp-bsp.h"

#include "board.h"

static const char *TAG = "board";

static temperature_sensor_handle_t temp_sensor = NULL;

void board_init()
{
    ESP_LOGI(TAG, "Initializing board");

    // Initialize board support package and LEDs
    bsp_i2c_init();
    bsp_leds_init();
    bsp_led_set(BSP_LED_RED, true);
    bsp_led_set(BSP_LED_BLUE, true);

    // Initialize temperature sensor
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
    ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_sensor));
    ESP_ERROR_CHECK(temperature_sensor_enable(temp_sensor));

    // Initialize codec board support (must be performed after BSP initialization)
    set_codec_board_type("S3_Korvo_V2");
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
}

float board_get_temp(void)
{
    float temp_out;
    ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_sensor, &temp_out));
    return temp_out;
}