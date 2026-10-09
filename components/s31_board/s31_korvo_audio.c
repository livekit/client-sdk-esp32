/*
 * Copyright 2026 Espressif Systems (Shanghai) CO LTD
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
 * Audio-only adapter derived from Espressif's ESP32-S31-Korvo BSP:
 * examples/esp32-s31-korvo/examples/common_components/esp32_s31_korvo
 * at esp-dev-kits commit df877cb1124a80835ad22fa5d8bafadb2348ce50.
 */

#include "sdkconfig.h"

#if CONFIG_IDF_TARGET_ESP32S31

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"

#include "s31_korvo_audio.h"

// Audio-only port of the Espressif Korvo BSP cited in the license header.
// Pin map: SDA/SCL 0/1; MCLK/BCLK/WS 2/3/4; codec input/output 5/6; PA 7.
// Capture and playback share the stereo bus but use separate ADC/DAC devices.
#define S31_KORVO_I2C_PORT       I2C_NUM_0
#define S31_KORVO_I2S_PORT       I2S_NUM_0
#define S31_KORVO_I2C_SDA        GPIO_NUM_0
#define S31_KORVO_I2C_SCL        GPIO_NUM_1
#define S31_KORVO_I2S_MCLK       GPIO_NUM_2
#define S31_KORVO_I2S_BCLK       GPIO_NUM_3
#define S31_KORVO_I2S_WS         GPIO_NUM_4
#define S31_KORVO_I2S_DOUT       GPIO_NUM_5
#define S31_KORVO_I2S_DIN        GPIO_NUM_6
#define S31_KORVO_PA_CTRL        GPIO_NUM_7
#define S31_KORVO_SAMPLE_RATE    48000

static const char *TAG = "s31_korvo_audio";

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
        .i2c_port = S31_KORVO_I2C_PORT,
        .sda_io_num = S31_KORVO_I2C_SDA,
        .scl_io_num = S31_KORVO_I2C_SCL,
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
        I2S_CHANNEL_DEFAULT_CONFIG(S31_KORVO_I2S_PORT, I2S_ROLE_MASTER);
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
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(S31_KORVO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(16, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = S31_KORVO_I2S_MCLK,
            .bclk = S31_KORVO_I2S_BCLK,
            .ws = S31_KORVO_I2S_WS,
            .dout = S31_KORVO_I2S_DOUT,
            .din = S31_KORVO_I2S_DIN,
        },
    };

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
        .port = S31_KORVO_I2S_PORT,
        .rx_handle = s_rx_channel,
        .tx_handle = s_tx_channel,
    };
    s_data_interface = audio_codec_new_i2s_data(&data_config);
    if (s_data_interface == NULL) {
        ESP_LOGE(TAG, "Failed to create codec I2S data interface");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "I2S0 ready: 48 kHz, 16-bit Philips stereo, MCLK/BCLK/WS=%d/%d/%d",
             S31_KORVO_I2S_MCLK, S31_KORVO_I2S_BCLK, S31_KORVO_I2S_WS);
    return ESP_OK;
}

static esp_codec_dev_handle_t create_codec_device(esp_codec_dev_type_t device_type,
                                                   esp_codec_dec_work_mode_t codec_mode,
                                                   int16_t pa_pin)
{
    const audio_codec_gpio_if_t *gpio_interface = audio_codec_new_gpio();
    if (gpio_interface == NULL) {
        ESP_LOGE(TAG, "Failed to create codec GPIO interface");
        return NULL;
    }

    audio_codec_i2c_cfg_t control_config = {
        .port = S31_KORVO_I2C_PORT,
        .addr = ES8389_CODEC_DEFAULT_ADDR,
        .bus_handle = s_i2c_bus,
    };
    const audio_codec_ctrl_if_t *control_interface =
        audio_codec_new_i2c_ctrl(&control_config);
    if (control_interface == NULL) {
        ESP_LOGE(TAG, "Failed to create codec I2C control interface");
        return NULL;
    }

    esp_codec_dev_hw_gain_t hardware_gain = {
        .pa_voltage = 5.0f,
        .codec_dac_voltage = 3.3f,
    };
    es8389_codec_cfg_t codec_config = {
        .ctrl_if = control_interface,
        .gpio_if = gpio_interface,
        .codec_mode = codec_mode,
        .pa_pin = pa_pin,
        .pa_reverted = false,
        .master_mode = false,
        .use_mclk = false,
        .digital_mic = false,
        .invert_mclk = false,
        .invert_sclk = false,
        .hw_gain = hardware_gain,
        .no_dac_ref = false,
    };
    const audio_codec_if_t *codec_interface = es8389_codec_new(&codec_config);
    if (codec_interface == NULL) {
        ESP_LOGE(TAG, "Failed to create ES8389 codec interface");
        return NULL;
    }

    esp_codec_dev_cfg_t device_config = {
        .dev_type = device_type,
        .codec_if = codec_interface,
        .data_if = s_data_interface,
    };
    return esp_codec_dev_new(&device_config);
}

esp_err_t s31_korvo_audio_init(void)
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

    s_playback_device = create_codec_device(
        ESP_CODEC_DEV_TYPE_OUT, ESP_CODEC_DEV_WORK_MODE_DAC, S31_KORVO_PA_CTRL);
    if (s_playback_device == NULL) {
        return ESP_FAIL;
    }
    s_record_device = create_codec_device(
        ESP_CODEC_DEV_TYPE_IN, ESP_CODEC_DEV_WORK_MODE_ADC, GPIO_NUM_NC);
    if (s_record_device == NULL) {
        return ESP_FAIL;
    }

    // Store the microphone gain before capture opens the codec device.
    if (esp_codec_dev_set_in_gain(s_record_device,
                                 CONFIG_LK_S31_KORVO_MIC_GAIN_DB) != ESP_CODEC_DEV_OK) {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "ES8389 speaker and microphone devices ready; PA enable is GPIO%d",
             S31_KORVO_PA_CTRL);
    return ESP_OK;
}

esp_codec_dev_handle_t s31_korvo_audio_get_playback_handle(void)
{
    return s_playback_device;
}

esp_codec_dev_handle_t s31_korvo_audio_get_record_handle(void)
{
    return s_record_device;
}

i2c_master_bus_handle_t s31_korvo_audio_get_i2c_bus_handle(void)
{
    return s_i2c_bus;
}

#endif // CONFIG_IDF_TARGET_ESP32S31
