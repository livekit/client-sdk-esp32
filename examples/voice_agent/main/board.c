#include "esp_log.h"
#include "codec_init.h"
#include "codec_board.h"
#include "driver/temperature_sensor.h"
#include "driver/gpio.h"
// Remove BSP dependency: #include "bsp/esp-bsp.h"

#include "board.h"

static const char *TAG = "board";

static temperature_sensor_handle_t temp_sensor = NULL;

// LED pin definitions for Waveshare ESP32-S3-Touch-AMOLED-1.8
#define LED_RED_PIN     GPIO_NUM_48   // RGB LED is on GPIO48 according to specs
#define LED_BLUE_PIN    GPIO_NUM_48   // Same RGB LED, different color control

void board_init()
{
    ESP_LOGI(TAG, "Initializing board for Waveshare ESP32-S3-Touch-AMOLED-1.8");

    // Initialize LED GPIO pins for Waveshare board (RGB LED on GPIO48)
    gpio_config_t led_config = {
        .pin_bit_mask = (1ULL << LED_RED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&led_config);
    
    // Turn on LED to indicate board is initializing
    gpio_set_level(LED_RED_PIN, 1);
    ESP_LOGI(TAG, "LED initialized on GPIO%d", LED_RED_PIN);

    // Initialize temperature sensor
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
    ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_sensor));
    ESP_ERROR_CHECK(temperature_sensor_enable(temp_sensor));
    ESP_LOGI(TAG, "Temperature sensor initialized");

    // Initialize codec board for Waveshare board
    set_codec_board_type("WAVESHARE_ESP32_S3_AMOLED_1_8");
    codec_init_cfg_t cfg = {
        .in_mode = CODEC_I2S_MODE_TDM,
        .in_use_tdm = true,
        .reuse_dev = false
    };
    if (init_codec(&cfg) != 0) {
        ESP_LOGE(TAG, "Failed to initialize codec");
    } else {
        ESP_LOGI(TAG, "Codec initialized successfully for Waveshare board");
    }
}

float board_get_temp(void)
{
    float temp_out;
    ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_sensor, &temp_out));
    return temp_out;
}