/*
 * Copyright 2026 LiveKit, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Audio wiring follows Espressif's ESP32-S31-Function-CoreBoard-1
 * schematic and esp-board-manager definition.
 */

#include "sdkconfig.h"

#if CONFIG_IDF_TARGET_ESP32S31

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"

#include "s31_function_core_audio.h"

// Audio port based on Espressif's Function-CoreBoard schematic and board definition.
// Pin map: SCL/SDA 50/51; MCLK/BCLK 52/53; codec output 54; WS 55;
// codec input 56; amplifier enable 57. Only the DAC controls the amplifier.
#define FUNCTION_CORE_I2C_PORT       I2C_NUM_0
#define FUNCTION_CORE_I2S_PORT       I2S_NUM_0
#define FUNCTION_CORE_I2C_SCL        GPIO_NUM_50
#define FUNCTION_CORE_I2C_SDA        GPIO_NUM_51
#define FUNCTION_CORE_I2S_MCLK       GPIO_NUM_52
#define FUNCTION_CORE_I2S_BCLK       GPIO_NUM_53
#define FUNCTION_CORE_I2S_DIN        GPIO_NUM_54
#define FUNCTION_CORE_I2S_WS         GPIO_NUM_55
#define FUNCTION_CORE_I2S_DOUT       GPIO_NUM_56
#define FUNCTION_CORE_PA_CTRL        GPIO_NUM_57
#define FUNCTION_CORE_SAMPLE_RATE    48000

static const char *TAG = "s31_function_audio";

static i2c_master_bus_handle_t s_i2c_bus;
static i2s_chan_handle_t s_tx_channel;
static i2s_chan_handle_t s_rx_channel;
static const audio_codec_data_if_t *s_data_interface;
static esp_codec_dev_handle_t s_playback_device;
static esp_codec_dev_handle_t s_record_device;

// --- Codec control bus ---
// Reuse the initialized bus rather than registering a second controller.
static esp_err_t init_i2c(void)
{
    if (s_i2c_bus != NULL) {
        return ESP_OK;
    }

    i2c_master_bus_config_t config = {
        .i2c_port = FUNCTION_CORE_I2C_PORT,
        .sda_io_num = FUNCTION_CORE_I2C_SDA,
        .scl_io_num = FUNCTION_CORE_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t ret = i2c_new_master_bus(&config, &s_i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C: %s", esp_err_to_name(ret));
    }
    return ret;
}

// --- Shared capture/playback bus ---
// Keep both directions on one native format to avoid live clock changes.
static esp_err_t init_i2s(void)
{
    if (s_data_interface != NULL) {
        return ESP_OK;
    }

    i2s_chan_config_t channel_config =
        I2S_CHANNEL_DEFAULT_CONFIG(FUNCTION_CORE_I2S_PORT, I2S_ROLE_MASTER);
    channel_config.auto_clear = true;
#if CONFIG_LK_EXAMPLE_ENABLE_AEC
    // AEC processes capture in bursts while playback shares the controller.
    // Keep the 240-frame DMA cadence, with 120 ms capacity for scheduling
    // stalls. This is queue capacity, not a delay before capture is delivered.
    channel_config.dma_desc_num = 24;
#endif

    esp_err_t ret = i2s_new_channel(&channel_config, &s_tx_channel, &s_rx_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create full-duplex I2S: %s", esp_err_to_name(ret));
        return ret;
    }

    i2s_std_config_t stream_config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(FUNCTION_CORE_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(16, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = FUNCTION_CORE_I2S_MCLK,
            .bclk = FUNCTION_CORE_I2S_BCLK,
            .ws = FUNCTION_CORE_I2S_WS,
            .dout = FUNCTION_CORE_I2S_DOUT,
            .din = FUNCTION_CORE_I2S_DIN,
        },
    };

    // Keep initial I2S setup and codec format changes on the same audio PLL.
    stream_config.clk_cfg.clk_src = I2S_CLK_SRC_APLL;
    ret = i2s_channel_init_std_mode(s_tx_channel, &stream_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2S TX: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = i2s_channel_init_std_mode(s_rx_channel, &stream_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2S RX: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = i2s_channel_enable(s_tx_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S TX: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = i2s_channel_enable(s_rx_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S RX: %s", esp_err_to_name(ret));
        return ret;
    }

    audio_codec_i2s_cfg_t data_config = {
        .port = FUNCTION_CORE_I2S_PORT,
        .clk_src = I2S_CLK_SRC_APLL,
        .rx_handle = s_rx_channel,
        .tx_handle = s_tx_channel,
    };
    s_data_interface = audio_codec_new_i2s_data(&data_config);
    if (s_data_interface == NULL) {
        ESP_LOGE(TAG, "Failed to create codec I2S data interface");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG,
             "I2S0 ready: 48 kHz, 16-bit Philips stereo, MCLK/BCLK/WS=%d/%d/%d",
             FUNCTION_CORE_I2S_MCLK, FUNCTION_CORE_I2S_BCLK,
             FUNCTION_CORE_I2S_WS);
    return ESP_OK;
}

static esp_err_t create_codec_devices(void)
{
    const audio_codec_gpio_if_t *gpio_interface = audio_codec_new_gpio();
    if (gpio_interface == NULL) {
        ESP_LOGE(TAG, "Failed to create codec GPIO interface");
        return ESP_FAIL;
    }

    audio_codec_i2c_cfg_t control_config = {
        .port = FUNCTION_CORE_I2C_PORT,
        .addr = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = s_i2c_bus,
    };
    const audio_codec_ctrl_if_t *control_interface =
        audio_codec_new_i2c_ctrl(&control_config);
    if (control_interface == NULL) {
        ESP_LOGE(TAG, "Failed to create codec I2C control interface");
        return ESP_FAIL;
    }

    es8311_codec_cfg_t codec_config = {
        .ctrl_if = control_interface,
        .gpio_if = gpio_interface,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .pa_pin = FUNCTION_CORE_PA_CTRL,
        .pa_reverted = false,
        .master_mode = false,
        .use_mclk = true,
        .digital_mic = false,
        .invert_mclk = false,
        .invert_sclk = false,
        .hw_gain.pa_gain = 6.0f,
        // AEC uses the separate internal DAC reference, not a second speaker.
#if CONFIG_LK_EXAMPLE_ENABLE_AEC
        .no_dac_ref = false,
#else
        .no_dac_ref = true,
#endif
    };
    const audio_codec_if_t *playback_interface = es8311_codec_new(&codec_config);
    if (playback_interface == NULL) {
        ESP_LOGE(TAG, "Failed to create ES8311 playback codec interface");
        return ESP_FAIL;
    }

    // The ES8311 driver tracks active ADC/DAC interfaces separately. Sharing
    // one BOTH interface lets closing playback suspend an active microphone.
    // Only the DAC interface owns the speaker amplifier GPIO.
    codec_config.codec_mode = ESP_CODEC_DEV_WORK_MODE_ADC;
    codec_config.pa_pin = GPIO_NUM_NC;
    const audio_codec_if_t *record_interface = es8311_codec_new(&codec_config);
    if (record_interface == NULL) {
        ESP_LOGE(TAG, "Failed to create ES8311 record codec interface");
        return ESP_FAIL;
    }

    esp_codec_dev_cfg_t playback_config = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = playback_interface,
        .data_if = s_data_interface,
    };
    s_playback_device = esp_codec_dev_new(&playback_config);
    if (s_playback_device == NULL) {
        ESP_LOGE(TAG, "Failed to create ES8311 playback device");
        return ESP_FAIL;
    }

    esp_codec_dev_cfg_t record_config = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
        .codec_if = record_interface,
        .data_if = s_data_interface,
    };
    s_record_device = esp_codec_dev_new(&record_config);
    if (s_record_device == NULL) {
        ESP_LOGE(TAG, "Failed to create ES8311 record device");
        return ESP_FAIL;
    }
    int ret = esp_codec_dev_set_in_gain(
        s_record_device, (float)CONFIG_LK_S31_FUNCTION_CORE_MIC_GAIN_DB);
    if (ret != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "Failed to set ES8311 microphone gain, ret=%d", ret);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t s31_function_core_audio_init(void)
{
    if (s_playback_device != NULL && s_record_device != NULL) {
        return ESP_OK;
    }

    esp_err_t ret = init_i2c();
    if (ret != ESP_OK) {
        return ret;
    }
    ret = init_i2s();
    if (ret != ESP_OK) {
        return ret;
    }
    ret = create_codec_devices();
    if (ret != ESP_OK) {
        return ret;
    }

    ESP_LOGI(TAG,
             "ES8311 mono speaker and microphone ready; PA enable is GPIO%d, microphone gain is %d dB",
             FUNCTION_CORE_PA_CTRL, CONFIG_LK_S31_FUNCTION_CORE_MIC_GAIN_DB);
    return ESP_OK;
}

esp_codec_dev_handle_t s31_function_core_audio_get_playback_handle(void)
{
    return s_playback_device;
}

esp_codec_dev_handle_t s31_function_core_audio_get_record_handle(void)
{
    return s_record_device;
}

#endif // CONFIG_IDF_TARGET_ESP32S31
