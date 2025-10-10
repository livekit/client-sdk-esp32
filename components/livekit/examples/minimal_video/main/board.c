#include "esp_log.h"
#include "board.h"
#include "codec_init.h"
#include "codec_board.h"
#include <math.h>

static const char *TAG = "board";

void board_init()
{
    ESP_LOGI(TAG, "Initializing board");

    set_codec_board_type(CONFIG_LK_EXAMPLE_CODEC_BOARD_TYPE);
    // Notes when use playback and record at same time, must set reuse_dev = false
    codec_init_cfg_t cfg = {
        .reuse_dev = false
    };
    init_codec(&cfg);
}