// Board initialization for the Waveshare ESP32-S3-Touch-LCD-1.83.
//
// This file manually configures I2C, I2S, and audio codecs (ES8311 + ES7210)
// using pin assignments extracted from the board schematic. It intentionally
// avoids the Waveshare BSP component to demonstrate how to bring up audio on
// any custom board.

#include "board.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "board";

// ---------------------------------------------------------------------------
// Pin definitions — Waveshare ESP32-S3-Touch-LCD-1.83
// Source: board schematic (ESP32-S3-Touch-LCD-1.83-schematic.pdf)
// ---------------------------------------------------------------------------

// I2C bus (shared by ES8311, ES7210, AXP2101, QMI8658, PCF85063, CST816D)
#define BOARD_I2C_SDA  GPIO_NUM_15
#define BOARD_I2C_SCL  GPIO_NUM_14

// I2S bus (shared by ES8311 and ES7210)
#define BOARD_I2S_MCLK GPIO_NUM_16
#define BOARD_I2S_BCLK GPIO_NUM_9
#define BOARD_I2S_WS   GPIO_NUM_45
#define BOARD_I2S_DOUT GPIO_NUM_8   // ESP32 → ES8311 (playback)
#define BOARD_I2S_DIN  GPIO_NUM_10  // ES7210 → ESP32 (recording)

// Speaker power amplifier enable
#define BOARD_PA_PIN   GPIO_NUM_46

// AXP2101 PMU — manages power rails for the audio codecs
#define AXP2101_ADDR        0x34   // 7-bit I2C address (direct ESP-IDF driver)
#define AXP2101_LDO_ONOFF   0x90   // ALDO1-4 / BLDO1-2 enable bits
#define AXP2101_ALDO1_VOLT  0x92   // ALDO1 voltage: val = (mV - 500) / 100

// Codec I2C addresses — 8-bit format (left-shifted) as required by esp_codec_dev.
// The driver internally right-shifts by 1 to obtain the 7-bit address.
//   ES8311: CE pin LOW → 7-bit 0x18, 8-bit 0x30 (default)
//   ES7210: AD0=AD1=0  → 7-bit 0x40, 8-bit 0x80 (default)
#define ES8311_ADDR    ES8311_CODEC_DEFAULT_ADDR   // 0x30
#define ES7210_ADDR    ES7210_CODEC_DEFAULT_ADDR   // 0x80

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

static i2c_master_bus_handle_t  i2c_bus;
static i2c_master_dev_handle_t pmu_dev;
static i2s_chan_handle_t        i2s_tx;
static i2s_chan_handle_t        i2s_rx;
static esp_codec_dev_handle_t   play_dev;
static esp_codec_dev_handle_t   rec_dev;

// ---------------------------------------------------------------------------
// Step 1: I2C
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// I2C bus scan (diagnostic) — probes every 7-bit address to list responding
// devices.  Useful when bringing up a new board.
// ---------------------------------------------------------------------------

static void i2c_bus_scan(void)
{
    ESP_LOGI(TAG, "Scanning I2C bus …");
    int found = 0;
    for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
        i2c_master_dev_handle_t probe;
        i2c_device_config_t cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address  = addr,
            .scl_speed_hz    = 100000,
        };
        if (i2c_master_bus_add_device(i2c_bus, &cfg, &probe) == ESP_OK) {
            esp_err_t err = i2c_master_probe(i2c_bus, addr, 50);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "  0x%02X — ACK", addr);
                found++;
            }
            i2c_master_bus_rm_device(probe);
        }
    }
    ESP_LOGI(TAG, "I2C scan complete — %d device(s) found", found);
}

// ---------------------------------------------------------------------------
// Step 2: AXP2101 PMU — ensure the ALDO1 rail (A3V3) that powers the audio
// codecs is enabled at 3.3 V.  On the Waveshare board the bootloader already
// enables all rails, but we set them explicitly so the code works standalone.
// ---------------------------------------------------------------------------

// Helper: write a single AXP2101 register
static esp_err_t pmu_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t cmd[] = { reg, val };
    return i2c_master_transmit(pmu_dev, cmd, sizeof(cmd), 1000);
}

static esp_err_t init_pmu(void)
{
    // Add the AXP2101 to the I2C bus
    i2c_device_config_t pmu_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = AXP2101_ADDR,
        .scl_speed_hz    = 400000,
    };
    ESP_RETURN_ON_ERROR(
        i2c_master_bus_add_device(i2c_bus, &pmu_cfg, &pmu_dev),
        TAG, "Failed to add AXP2101 to I2C bus");

    // ALDO1 → 3.3 V  (register value: (3300 − 500) / 100 = 0x1C)
    pmu_write_reg(AXP2101_ALDO1_VOLT, 0x1C);

    // Enable ALDO1 (bit 0 of register 0x90)
    uint8_t en_val = 0;
    uint8_t reg = AXP2101_LDO_ONOFF;
    i2c_master_transmit_receive(pmu_dev, &reg, 1, &en_val, 1, 1000);
    en_val |= 0x01;
    pmu_write_reg(AXP2101_LDO_ONOFF, en_val);

    // Allow the power rail to stabilize
    vTaskDelay(pdMS_TO_TICKS(20));

    ESP_LOGI(TAG, "AXP2101: ALDO1 enabled at 3.3 V (codec power)");
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// Step 3: I2S (TDM for ES7210 4-channel input, standard for ES8311 output)
// ---------------------------------------------------------------------------

static esp_err_t init_i2s(void)
{
    // Create a duplex channel (TX + RX on the same I2S port)
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    ESP_RETURN_ON_ERROR(
        i2s_new_channel(&chan_cfg, &i2s_tx, &i2s_rx),
        TAG, "Failed to create I2S channel");

    // TX: standard mode for ES8311 playback
    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(32, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = BOARD_I2S_MCLK,
            .bclk = BOARD_I2S_BCLK,
            .ws   = BOARD_I2S_WS,
            .dout = BOARD_I2S_DOUT,
            .din  = BOARD_I2S_DIN,
        },
    };
    ESP_RETURN_ON_ERROR(
        i2s_channel_init_std_mode(i2s_tx, &std_cfg),
        TAG, "Failed to init I2S TX (std)");

    // RX: TDM mode for ES7210 4-channel recording
    i2s_tdm_slot_mask_t slot_mask =
        I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3;
    i2s_tdm_config_t tdm_cfg = {
        .clk_cfg  = I2S_TDM_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_TDM_PHILIPS_SLOT_DEFAULT_CONFIG(32, I2S_SLOT_MODE_STEREO, slot_mask),
        .gpio_cfg = {
            .mclk = BOARD_I2S_MCLK,
            .bclk = BOARD_I2S_BCLK,
            .ws   = BOARD_I2S_WS,
            .dout = BOARD_I2S_DOUT,
            .din  = BOARD_I2S_DIN,
        },
    };
    tdm_cfg.slot_cfg.total_slot = 4;
    ESP_RETURN_ON_ERROR(
        i2s_channel_init_tdm_mode(i2s_rx, &tdm_cfg),
        TAG, "Failed to init I2S RX (TDM)");

    // Enable both channels — some codecs need the clock running during register
    // configuration.
    i2s_channel_enable(i2s_tx);
    i2s_channel_enable(i2s_rx);

    return ESP_OK;
}

// ---------------------------------------------------------------------------
// Step 4: ES8311 (DAC — speaker output)
// ---------------------------------------------------------------------------

static esp_err_t init_es8311(void)
{
    // I2C control interface
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port       = I2C_NUM_0,
        .bus_handle = i2c_bus,
        .addr       = ES8311_ADDR,
    };
    const audio_codec_ctrl_if_t *ctrl = audio_codec_new_i2c_ctrl(&i2c_cfg);
    ESP_RETURN_ON_FALSE(ctrl, ESP_FAIL, TAG, "ES8311 I2C ctrl failed");

    // GPIO interface for PA control
    const audio_codec_gpio_if_t *gpio = audio_codec_new_gpio();
    ESP_RETURN_ON_FALSE(gpio, ESP_FAIL, TAG, "GPIO interface failed");

    // ES8311 codec driver
    es8311_codec_cfg_t codec_cfg = {
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .ctrl_if    = ctrl,
        .gpio_if    = gpio,
        .pa_pin     = BOARD_PA_PIN,
        .use_mclk   = true,
        .hw_gain    = { .pa_gain = 6.0 },
    };
    const audio_codec_if_t *codec = es8311_codec_new(&codec_cfg);
    ESP_RETURN_ON_FALSE(codec, ESP_FAIL, TAG, "ES8311 codec init failed");

    // I2S data interface
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port      = I2S_NUM_0,
        .tx_handle = i2s_tx,
        .rx_handle = i2s_rx,
    };
    const audio_codec_data_if_t *data = audio_codec_new_i2s_data(&i2s_cfg);
    ESP_RETURN_ON_FALSE(data, ESP_FAIL, TAG, "I2S data interface failed");

    // Playback device
    esp_codec_dev_cfg_t dev_cfg = {
        .codec_if = codec,
        .data_if  = data,
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
    };
    play_dev = esp_codec_dev_new(&dev_cfg);
    ESP_RETURN_ON_FALSE(play_dev, ESP_FAIL, TAG, "Playback device creation failed");

    esp_codec_dev_set_out_vol(play_dev, CONFIG_LK_EXAMPLE_SPEAKER_VOLUME);
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// Step 5: ES7210 (ADC — microphone input)
// ---------------------------------------------------------------------------

static esp_err_t init_es7210(void)
{
    // I2C control interface
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port       = I2C_NUM_0,
        .bus_handle = i2c_bus,
        .addr       = ES7210_ADDR,
    };
    const audio_codec_ctrl_if_t *ctrl = audio_codec_new_i2c_ctrl(&i2c_cfg);
    ESP_RETURN_ON_FALSE(ctrl, ESP_FAIL, TAG, "ES7210 I2C ctrl failed");

    // ES7210 codec driver — select all 4 mic channels for TDM
    es7210_codec_cfg_t codec_cfg = {
        .ctrl_if     = ctrl,
        .mic_selected = ES7210_SEL_MIC1 | ES7210_SEL_MIC2 |
                        ES7210_SEL_MIC3 | ES7210_SEL_MIC4,
    };
    const audio_codec_if_t *codec = es7210_codec_new(&codec_cfg);
    ESP_RETURN_ON_FALSE(codec, ESP_FAIL, TAG, "ES7210 codec init failed");

    // I2S data interface (shares the same I2S port as ES8311)
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port      = I2S_NUM_0,
        .rx_handle = i2s_rx,
    };
    const audio_codec_data_if_t *data = audio_codec_new_i2s_data(&i2s_cfg);
    ESP_RETURN_ON_FALSE(data, ESP_FAIL, TAG, "I2S data interface (in) failed");

    // Record device
    esp_codec_dev_cfg_t dev_cfg = {
        .codec_if = codec,
        .data_if  = data,
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
    };
    rec_dev = esp_codec_dev_new(&dev_cfg);
    ESP_RETURN_ON_FALSE(rec_dev, ESP_FAIL, TAG, "Record device creation failed");

    esp_codec_dev_set_in_gain(rec_dev, 30.0);
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void board_init(void)
{
    ESP_LOGI(TAG, "Initializing Waveshare ESP32-S3-Touch-LCD-1.83");

    ESP_ERROR_CHECK(init_i2c());
    ESP_ERROR_CHECK(init_pmu());
    i2c_bus_scan();   // diagnostic — shows every device on the bus
    ESP_ERROR_CHECK(init_i2s());
    ESP_ERROR_CHECK(init_es8311());
    ESP_ERROR_CHECK(init_es7210());

    ESP_LOGI(TAG, "Board init complete — ES8311 (playback) + ES7210 (record) ready");
}

esp_codec_dev_handle_t get_playback_handle(void)
{
    return play_dev;
}

esp_codec_dev_handle_t get_record_handle(void)
{
    return rec_dev;
}
