#include "esp_log.h"
#include "board.h"
#include "codec_init.h"
#include "codec_board.h"
#include "miniscale.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "scale.h"
#include <math.h>

static const char *TAG = "board";

void board_init()
{
    ESP_LOGI(TAG, "Initializing board");

    set_codec_board_type("ESP32_P4_DEV_V14");
    // Notes when use playback and record at same time, must set reuse_dev = false
    codec_init_cfg_t cfg = {
        .reuse_dev = false
    };
    init_codec(&cfg);
    board_lcd_init();

    scale_init((i2c_master_bus_handle_t)get_i2c_bus_handle(0));
}