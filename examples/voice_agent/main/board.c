#include "esp_log.h"
#include "codec_init.h"
#include "codec_board.h"
#include "driver/temperature_sensor.h"
#include "bsp/esp-bsp.h"
#include "sdkconfig.h"
#include <string.h>

#include "board.h"

static const char *TAG = "board";

static temperature_sensor_handle_t temp_sensor = NULL;

// Check if LED functions are available (not available in EchoEar BSP)
extern void bsp_leds_init(void) __attribute__((weak));
extern void bsp_led_set(int led, bool state) __attribute__((weak));

// LED constants (might not be defined in EchoEar BSP)
#ifndef BSP_LED_RED
#define BSP_LED_RED 0
#endif
#ifndef BSP_LED_BLUE  
#define BSP_LED_BLUE 1
#endif

void board_init()
{
    ESP_LOGI(TAG, "Initializing board for %s", CONFIG_CODEC_BOARD_TYPE);

    // Initialize board support package
    bsp_i2c_init();
    
    // Only initialize LEDs if LED functions are available (not in EchoEar BSP)
    if (bsp_leds_init != NULL) {
        ESP_LOGI(TAG, "Initializing LEDs for %s", CONFIG_CODEC_BOARD_TYPE);
        bsp_leds_init();
        if (bsp_led_set != NULL) {
            bsp_led_set(BSP_LED_RED, true);
            bsp_led_set(BSP_LED_BLUE, true);
        }
    } else {
        ESP_LOGI(TAG, "LED functions not available for %s (EchoEar has no external LEDs)", CONFIG_CODEC_BOARD_TYPE);
    }

    // Initialize temperature sensor
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
    ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_sensor));
    ESP_ERROR_CHECK(temperature_sensor_enable(temp_sensor));

    // Initialize codec board
    set_codec_board_type(CONFIG_CODEC_BOARD_TYPE);
    codec_init_cfg_t cfg = {
        .in_mode = CODEC_I2S_MODE_TDM,
        .in_use_tdm = true,
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