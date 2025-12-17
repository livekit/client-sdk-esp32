#include "esp_log.h"
#include "esp_check.h"

#include "bsp/esp32_s3_touch_amoled_2_06.h"

#include "board.h"

static const char *TAG = "board";

// Some IDE/lint setups don't automatically include the generated `sdkconfig.h`.
// Keep this as a harmless fallback to avoid false-positive diagnostics.
#ifndef CONFIG_LK_EXAMPLE_CODEC_BOARD_TYPE
#define CONFIG_LK_EXAMPLE_CODEC_BOARD_TYPE "UNUSED"
#endif

// This example uses the Waveshare ESP32-S3-Touch-AMOLED-2.06 BSP to initialize audio I/O.
static esp_codec_dev_handle_t s_mic_handle = NULL;
static esp_codec_dev_handle_t s_spk_handle = NULL;

void *board_get_mic_handle(void)
{
    return s_mic_handle;
}

void *board_get_speaker_handle(void)
{
    return s_spk_handle;
}

void board_init(void)
{
    ESP_LOGI(TAG, "Initializing board");

    // Initialize I2C (needed for codec + touch, etc.)
    ESP_ERROR_CHECK(bsp_i2c_init());

    // Initialize audio (I2S + codec devices) via BSP
    const i2s_std_config_t i2s_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = BSP_I2S_MCLK,
            .bclk = BSP_I2S_SCLK,
            .ws = BSP_I2S_LCLK,
            .dout = BSP_I2S_DOUT,
            .din = BSP_I2S_DSIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    ESP_ERROR_CHECK(bsp_audio_init(&i2s_cfg));

    s_spk_handle = bsp_audio_codec_speaker_init();
    ESP_RETURN_VOID_ON_FALSE(s_spk_handle != NULL, TAG, "Failed to init speaker codec device");

    s_mic_handle = bsp_audio_codec_microphone_init();
    ESP_RETURN_VOID_ON_FALSE(s_mic_handle != NULL, TAG, "Failed to init microphone codec device");
}