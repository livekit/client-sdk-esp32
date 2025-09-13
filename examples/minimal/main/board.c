#include "esp_log.h"
#include "codec_init.h"
#include "codec_board.h"
#include "board.h"

static const char *TAG = "board";

void board_init()
{
    ESP_LOGI(TAG, "Initializing board");

    // Initialize codec board
    set_codec_board_type(CONFIG_LK_EXAMPLE_CODEC_BOARD_TYPE);
    codec_init_cfg_t cfg = {
        .in_mode = CODEC_I2S_MODE_TDM,
        .in_use_tdm = true,
        .reuse_dev = false
    };
    init_codec(&cfg);
}