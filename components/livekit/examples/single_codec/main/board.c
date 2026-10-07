// Board initialization for the GMIC HA-TOYMD module (ESP32-S3 + ES8311).
//
// Pin assignments verified on real hardware: the microphone captures live audio
// and the speaker plays back. One codec handles both directions - there is no
// second ADC chip and no power-management IC on this module.
//
//   I2C   SDA 17   SCL 18        ES8311 at 7-bit 0x18
//   I2S   MCLK 16  BCLK 9  WS 45  DOUT 8 (to speaker)  DIN 10 (from mic)
//   Amp   enable GPIO 48
//
// Format is 16-bit Philips I2S at 16 kHz, which is what the codec was verified
// working with.

#include "board.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "board";

#define BOARD_I2C_SDA  GPIO_NUM_17
#define BOARD_I2C_SCL  GPIO_NUM_18

#define BOARD_I2S_MCLK GPIO_NUM_16
#define BOARD_I2S_BCLK GPIO_NUM_9
#define BOARD_I2S_WS   GPIO_NUM_45
#define BOARD_I2S_DOUT GPIO_NUM_8    // ESP32 -> ES8311 (playback)
#define BOARD_I2S_DIN  GPIO_NUM_10   // ES8311 -> ESP32 (microphone)

#define BOARD_PA_PIN   GPIO_NUM_48   // speaker amplifier enable

#define BOARD_SAMPLE_RATE 16000
#define BOARD_MCLK_MULT   256

static i2c_master_bus_handle_t i2c_bus;
static i2s_chan_handle_t       i2s_tx;
static i2s_chan_handle_t       i2s_rx;
static esp_codec_dev_handle_t  play_dev;
static esp_codec_dev_handle_t  rec_dev;

static esp_err_t init_i2c(void)
{
    i2c_master_bus_config_t cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port   = I2C_NUM_0,
        .scl_io_num = BOARD_I2C_SCL,
        .sda_io_num = BOARD_I2C_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    return i2c_new_master_bus(&cfg, &i2c_bus);
}

// Diagnostic: list every device that answers on the bus. On a healthy module
// this prints exactly one line, 0x18 (the ES8311).
static void i2c_bus_scan(void)
{
    ESP_LOGI(TAG, "Scanning I2C bus ...");
    int found = 0;
    for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
        if (i2c_master_probe(i2c_bus, addr, 50) == ESP_OK) {
            ESP_LOGI(TAG, "  0x%02X - ACK", addr);
            found++;
        }
    }
    ESP_LOGI(TAG, "I2C scan complete - %d device(s)", found);
}

// The codec needs its clocks running before it will accept register writes,
// so I2S is brought up first and left enabled.
static esp_err_t init_i2s(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &i2s_tx, &i2s_rx),
                        TAG, "Failed to create I2S channel");

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(BOARD_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                        I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = BOARD_I2S_MCLK,
            .bclk = BOARD_I2S_BCLK,
            .ws   = BOARD_I2S_WS,
            .dout = BOARD_I2S_DOUT,
            .din  = BOARD_I2S_DIN,
        },
    };
    std_cfg.clk_cfg.mclk_multiple = BOARD_MCLK_MULT;

    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(i2s_tx, &std_cfg),
                        TAG, "Failed to init I2S TX");
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(i2s_rx, &std_cfg),
                        TAG, "Failed to init I2S RX");
    i2s_channel_enable(i2s_tx);
    i2s_channel_enable(i2s_rx);
    return ESP_OK;
}

// One ES8311 serves both directions: two esp_codec_dev handles share the same
// codec and data interface, one for playback and one for capture.
static esp_err_t init_codec(void)
{
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port       = I2C_NUM_0,
        .bus_handle = i2c_bus,
        .addr       = ES8311_CODEC_DEFAULT_ADDR,
    };
    const audio_codec_ctrl_if_t *ctrl = audio_codec_new_i2c_ctrl(&i2c_cfg);
    ESP_RETURN_ON_FALSE(ctrl, ESP_FAIL, TAG, "ES8311 I2C ctrl failed");

    const audio_codec_gpio_if_t *gpio = audio_codec_new_gpio();
    ESP_RETURN_ON_FALSE(gpio, ESP_FAIL, TAG, "GPIO interface failed");

    es8311_codec_cfg_t codec_cfg = {
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH,
        .ctrl_if    = ctrl,
        .gpio_if    = gpio,
        .pa_pin     = BOARD_PA_PIN,
        .use_mclk   = true,
        .mclk_div   = BOARD_MCLK_MULT,
        .hw_gain    = { .pa_voltage = 5.0, .codec_dac_voltage = 3.3 },
    };
    const audio_codec_if_t *codec = es8311_codec_new(&codec_cfg);
    ESP_RETURN_ON_FALSE(codec, ESP_FAIL, TAG, "ES8311 codec init failed");

    audio_codec_i2s_cfg_t i2s_cfg = {
        .port      = I2S_NUM_0,
        .tx_handle = i2s_tx,
        .rx_handle = i2s_rx,
    };
    const audio_codec_data_if_t *data = audio_codec_new_i2s_data(&i2s_cfg);
    ESP_RETURN_ON_FALSE(data, ESP_FAIL, TAG, "I2S data interface failed");

    esp_codec_dev_cfg_t out_cfg = {
        .codec_if = codec, .data_if = data, .dev_type = ESP_CODEC_DEV_TYPE_OUT,
    };
    play_dev = esp_codec_dev_new(&out_cfg);
    ESP_RETURN_ON_FALSE(play_dev, ESP_FAIL, TAG, "Playback device failed");

    esp_codec_dev_cfg_t in_cfg = {
        .codec_if = codec, .data_if = data, .dev_type = ESP_CODEC_DEV_TYPE_IN,
    };
    rec_dev = esp_codec_dev_new(&in_cfg);
    ESP_RETURN_ON_FALSE(rec_dev, ESP_FAIL, TAG, "Record device failed");

    esp_codec_dev_set_out_vol(play_dev, CONFIG_LK_EXAMPLE_SPEAKER_VOLUME);
    esp_codec_dev_set_in_gain(rec_dev, 30.0);
    return ESP_OK;
}

void board_init(void)
{
    ESP_LOGI(TAG, "Initializing GMIC HA-TOYMD (ESP32-S3 + ES8311)");
    ESP_ERROR_CHECK(init_i2c());
    ESP_ERROR_CHECK(init_i2s());
    i2c_bus_scan();
    ESP_ERROR_CHECK(init_codec());
    ESP_LOGI(TAG, "Board init complete - ES8311 playback + capture ready");
}

esp_codec_dev_handle_t get_playback_handle(void) { return play_dev; }
esp_codec_dev_handle_t get_record_handle(void)   { return rec_dev; }
