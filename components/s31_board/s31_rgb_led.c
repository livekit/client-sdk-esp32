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
 */

// Share the single-pixel RGB controls across S31 board profiles while leaving
// the LED GPIO choice to each board adapter.

#include <string.h>

#include "esp_log.h"
#include "led_strip.h"

#include "s31_rgb_led.h"

// Keep the status indicator at a modest brightness, including white where
// all three channels are active.
#define S31_RGB_LED_BRIGHTNESS 32

static const char *TAG = "s31_rgb_led";
static led_strip_handle_t led_strip;

esp_err_t s31_rgb_led_init(int gpio_num)
{
    if (led_strip != NULL) {
        return ESP_OK;
    }

    const led_strip_config_t strip_config = {
        .strip_gpio_num = gpio_num,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        },
    };
    const led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 0,
        .flags = {
            .with_dma = false,
        },
    };

    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create WS2812 device on GPIO%d: %s",
                 gpio_num, esp_err_to_name(ret));
        return ret;
    }
    ret = led_strip_clear(led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to clear WS2812 LED: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Initialized one WS2812 RGB LED on GPIO%d", gpio_num);
    return ESP_OK;
}

bool s31_rgb_led_set(const char *color, bool state)
{
    if (led_strip == NULL || color == NULL) {
        return false;
    }
    // An off command clears the whole pixel regardless of its previous color.
    if (!state) {
        return led_strip_clear(led_strip) == ESP_OK;
    }

    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
    if (strcmp(color, "red") == 0) {
        red = S31_RGB_LED_BRIGHTNESS;
    } else if (strcmp(color, "green") == 0) {
        green = S31_RGB_LED_BRIGHTNESS;
    } else if (strcmp(color, "blue") == 0) {
        blue = S31_RGB_LED_BRIGHTNESS;
    } else if (strcmp(color, "white") == 0) {
        red = S31_RGB_LED_BRIGHTNESS;
        green = S31_RGB_LED_BRIGHTNESS;
        blue = S31_RGB_LED_BRIGHTNESS;
    } else {
        return false;
    }

    esp_err_t ret = led_strip_set_pixel(led_strip, 0, red, green, blue);
    if (ret == ESP_OK) {
        ret = led_strip_refresh(led_strip);
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WS2812 LED: %s", esp_err_to_name(ret));
    }
    return ret == ESP_OK;
}
