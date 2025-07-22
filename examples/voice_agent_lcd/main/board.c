#include "esp_log.h"
#include "codec_init.h"
#include "codec_board.h"
#include "driver/temperature_sensor.h"
#include "bsp/esp-bsp.h"
#include "lvgl.h"

#include "board.h"

static const char *TAG = "board";

static temperature_sensor_handle_t temp_sensor = NULL;

extern void example_ui(lv_obj_t *scr);

void board_init()
{
    ESP_LOGI(TAG, "Initializing board");

    // Initialize board support package, LCD, and UI.
    bsp_display_start();

    bsp_display_lock(0);
    lv_obj_t *scr = lv_disp_get_scr_act(NULL);
    example_ui(scr);

    bsp_display_unlock();
    bsp_display_backlight_on();

    // Initialize codec board
    set_codec_board_type(CONFIG_CODEC_BOARD_TYPE);
    codec_init_cfg_t cfg = {
        .in_mode = CODEC_I2S_MODE_TDM,
        .in_use_tdm = true,
        .reuse_dev = false
    };
    init_codec(&cfg);

    // Initialize temperature sensor
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
    ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_sensor));
    ESP_ERROR_CHECK(temperature_sensor_enable(temp_sensor));
}

float board_get_temp(void)
{
    float temp_out;
    ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_sensor, &temp_out));
    return temp_out;
}