#include "esp_log.h"
#include "codec_init.h"
#include "codec_board.h"
#include "bsp/esp-bsp.h"

#include "board.h"

static const char *TAG = "board";

void board_init()
{
    ESP_LOGI(TAG, "Initializing board");

    bsp_i2c_init();
    bsp_power_init(true);

    bsp_display_cfg_t display_cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * CONFIG_BSP_LCD_DRAW_BUF_HEIGHT,
        .double_buffer = true,
        .flags = {
            .buff_dma = true,
            .buff_spiram = false
        }
    };
    display_cfg.lvgl_port_cfg.task_affinity = 1; // Put LVGL on core 1
    bsp_display_start_with_config(&display_cfg);

    bsp_display_backlight_on();

    // Initialize codec board
    set_codec_board_type("ESP32_S3_EchoEar");
    codec_init_cfg_t cfg = {
        .in_mode = CODEC_I2S_MODE_TDM,
        .in_use_tdm = true,
        .reuse_dev = false
    };
    init_codec(&cfg);
}