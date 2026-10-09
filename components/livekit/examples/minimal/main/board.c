#include "esp_log.h"
#if CONFIG_IDF_TARGET_ESP32S31
#include "livekit_s31_board.h"
#else
#include "codec_init.h"
#include "codec_board.h"
#endif
#include "board.h"

static const char *TAG = "board";

void board_init()
{
    ESP_LOGI(TAG, "Initializing board");

    // S31 uses the reusable LiveKit board profiles. Established targets keep
    // using the codec_board component.
#if CONFIG_IDF_TARGET_ESP32S31
    esp_err_t ret = livekit_s31_board_init(CONFIG_LK_EXAMPLE_CODEC_BOARD_TYPE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize S31 board: %s",
                 esp_err_to_name(ret));
    }
#else
    set_codec_board_type(CONFIG_LK_EXAMPLE_CODEC_BOARD_TYPE);
    codec_init_cfg_t cfg = {
        .in_mode = CODEC_I2S_MODE_TDM,
        .in_use_tdm = true,
        .reuse_dev = false
    };
    init_codec(&cfg);
#endif
}

esp_codec_dev_handle_t board_get_playback_handle(void)
{
#if CONFIG_IDF_TARGET_ESP32S31
    return livekit_s31_board_get_playback_handle();
#else
    return get_playback_handle();
#endif
}

esp_codec_dev_handle_t board_get_record_handle(void)
{
#if CONFIG_IDF_TARGET_ESP32S31
    return livekit_s31_board_get_record_handle();
#else
    return get_record_handle();
#endif
}

unsigned board_get_playback_channels(void)
{
#if CONFIG_IDF_TARGET_ESP32S31
    return livekit_s31_board_get_playback_channels();
#else
    return 2U;
#endif
}
