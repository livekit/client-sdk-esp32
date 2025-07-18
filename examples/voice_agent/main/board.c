#include "esp_log.h"
#include "codec_init.h"
#include "codec_board.h"
#include "driver/temperature_sensor.h"
#include "bsp/esp-bsp.h"

// Adding include for string comparison to check board type dynamically.
// This is required for conditional configuration based on CONFIG_CODEC_BOARD_TYPE.

#include <string.h>

#include "board.h"

static const char *TAG = "board";

static temperature_sensor_handle_t temp_sensor = NULL;

void board_init()
{
    ESP_LOGI(TAG, "Initializing board");

    // Condition BSP initializations for Korvo-specific hardware.
    // The Waveshare board does not require or support these BSP calls,
    // so they are wrapped to enable seamless switching between boards.
    // This change is necessary because the Waveshare device has different peripherals,
    // and using Korvo-specific BSP would cause failures.

    if (strcmp(CONFIG_CODEC_BOARD_TYPE, "S3_Korvo_V2") == 0) {
        bsp_i2c_init();
        bsp_leds_init();
        bsp_led_set(BSP_LED_RED, true);
        bsp_led_set(BSP_LED_BLUE, true);
    }

    // Initialize temperature sensor
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
    ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_sensor));
    ESP_ERROR_CHECK(temperature_sensor_enable(temp_sensor));

    // Initialize codec board support (must be performed after BSP initialization)
    set_codec_board_type(CONFIG_CODEC_BOARD_TYPE);
    // When using performing recording and playback at the same time,
    // reuse_dev must be set to false.
    codec_init_cfg_t cfg = {
        .reuse_dev = false
    };

    // Set I2S mode based on board type for compatibility.
    // Korvo uses TDM mode for its ES7210 multi-microphone input.
    // Waveshare uses STD mode as ES8311 is a simple mono codec and does not support TDM,
    // aligning with the working i2s_example for this board.
    // This dynamic configuration allows easy switching without breaking existing setups.

#if CONFIG_IDF_TARGET_ESP32S3
    if (strcmp(CONFIG_CODEC_BOARD_TYPE, "S3_Korvo_V2") == 0) {
        cfg.in_mode = CODEC_I2S_MODE_TDM;
        cfg.in_use_tdm = true;
    } else if (strcmp(CONFIG_CODEC_BOARD_TYPE, "WAVESHARE_S3_TOUCH_AMOLED") == 0) {
        cfg.in_mode = CODEC_I2S_MODE_STD;
        cfg.in_use_tdm = false;
    }
#endif

    init_codec(&cfg);
}

float board_get_temp(void)
{
    float temp_out;
    ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_sensor, &temp_out));
    return temp_out;
}