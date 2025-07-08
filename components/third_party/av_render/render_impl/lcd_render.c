/**
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2025 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#include <sdkconfig.h>
#include <inttypes.h>
#include "video_render.h"
#include "av_render_default.h"
#include "media_lib_os.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#if SOC_LCD_RGB_SUPPORTED
#include "esp_lcd_panel_rgb.h"
#endif
#if CONFIG_IDF_TARGET_ESP32P4
#include "esp_lcd_mipi_dsi.h"
#endif
#include "esp_timer.h"

#define TAG "LCD_RENDER"

typedef struct {
    av_render_video_frame_info_t info;
    esp_lcd_panel_handle_t       handle;
    bool                         rgb_panel;
    bool                         dsi_panel;
    uint8_t                     *frame_buffer[2];
    bool                         sel;
    uint32_t                     start_time;
    uint8_t                      frame_num;
    bool                         drawing;
} lcd_render_t;

static int lcd_render_close(video_render_handle_t h);

#if CONFIG_IDF_TARGET_ESP32P4
static bool draw_finished(esp_lcd_panel_handle_t panel, esp_lcd_dpi_panel_event_data_t *data, void *ctx)
{
    lcd_render_t *lcd = (lcd_render_t *)ctx;
    lcd->drawing      = false;
    return true;
}
#endif

static video_render_handle_t lcd_render_open(void *cfg, int size)
{
    lcd_render_cfg_t *lcd_cfg = (lcd_render_cfg_t *)cfg;
    if (cfg == NULL || size != sizeof(lcd_render_cfg_t) || lcd_cfg->lcd_handle == NULL) {
        ESP_LOGE(TAG, "Invalid argument to open");
        return NULL;
    }
    lcd_render_t *lcd = (lcd_render_t *)media_lib_calloc(1, sizeof(lcd_render_t));
    if (lcd == NULL) {
        return NULL;
    }
    lcd->handle = lcd_cfg->lcd_handle; // lcd_cfg->lcd_handle;
    lcd->rgb_panel = lcd_cfg->rgb_panel;
    lcd->dsi_panel = lcd_cfg->dsi_panel;
    if (lcd_cfg->use_frame_buffer) {
        if (lcd->rgb_panel) {
#if SOC_LCD_RGB_SUPPORTED
            esp_lcd_rgb_panel_get_frame_buffer(lcd->handle, 2, (void **)&lcd->frame_buffer[0],
                                               (void **)&lcd->frame_buffer[1]);
#endif
        }
        if (lcd->dsi_panel) {
#if CONFIG_IDF_TARGET_ESP32P4
            esp_lcd_dpi_panel_get_frame_buffer(lcd->handle, 2, (void **)&lcd->frame_buffer[0],
                                               (void **)&lcd->frame_buffer[1]);
#endif
        }
        if (lcd->frame_buffer[0] == NULL) {
            ESP_LOGE(TAG, "Fail to get frame buffer");
            lcd_render_close(lcd);
            return NULL;
        }
    }
#if CONFIG_IDF_TARGET_ESP32P4
    esp_lcd_dpi_panel_event_callbacks_t dpi_cb = {
        .on_color_trans_done = draw_finished,
    };
    esp_lcd_dpi_panel_register_event_callbacks(lcd->handle, &dpi_cb, lcd);
#endif
    return lcd;
}

static bool lcd_render_format_supported(video_render_handle_t h, av_render_video_frame_type_t frame_type)
{
    if (h == NULL) {
        return ESP_MEDIA_ERR_INVALID_ARG;
    }
    lcd_render_t *lcd = (lcd_render_t *)h;
    if (lcd->rgb_panel) {
        if (frame_type == AV_RENDER_VIDEO_RAW_TYPE_RGB565) {
            return true;
        }
    } else {
#if CONFIG_IDF_TARGET_ESP32P4
        if (frame_type == AV_RENDER_VIDEO_RAW_TYPE_RGB565) {
            return true;
        }
#else
        if (frame_type == AV_RENDER_VIDEO_RAW_TYPE_RGB565_BE) {
            return true;
        }
#endif
    }
    return false;
}

static int lcd_render_set_frame_info(video_render_handle_t h, av_render_video_frame_info_t *info)
{
    if (h == NULL || info == NULL) {
        return ESP_MEDIA_ERR_INVALID_ARG;
    }
    if (lcd_render_format_supported(h, info->type) == false) {
        ESP_LOGE(TAG, "Only support format %d", info->type);
        return ESP_MEDIA_ERR_NOT_SUPPORT;
    }
    lcd_render_t *lcd = (lcd_render_t *)h;
    memcpy(&lcd->info, info, sizeof(av_render_video_frame_info_t));
    ESP_LOGI(TAG, "Render started %dx%d", info->width, info->height);
    return 0;
}

static int lcd_render_write(video_render_handle_t h, av_render_video_frame_t *video_data)
{
    lcd_render_t *lcd = (lcd_render_t *)h;
    if (lcd == NULL || video_data == NULL || video_data->size == 0) {
        ESP_LOGE(TAG, "lcd %p data %p size %d", lcd, video_data, video_data->size);
        return ESP_MEDIA_ERR_INVALID_ARG;
    }
    if (lcd->start_time == 0 || lcd->frame_num == 0) {
        lcd->start_time = esp_timer_get_time();
    }
    lcd->frame_num++;
    uint32_t cur_time = esp_timer_get_time();
    if (cur_time > lcd->start_time + 1000000) {
        uint32_t elapse = cur_time - lcd->start_time;
        ESP_LOGI(TAG, "fps: %" PRIu32, lcd->frame_num * 1000000 / elapse);
        lcd->start_time = cur_time;
        lcd->frame_num = 0;
    }
#if CONFIG_IDF_TARGET_ESP32P4
    while (lcd->drawing) {
        media_lib_thread_sleep(10);
    }
    lcd->drawing = true;
#endif
    if (lcd->info.type == AV_RENDER_VIDEO_RAW_TYPE_RGB565 || lcd->info.type == AV_RENDER_VIDEO_RAW_TYPE_RGB565_BE) {
        uint8_t *rgb_data = video_data->data;
        ESP_LOGD(TAG, "pts:%d size %d", (int)video_data->pts, (int)video_data->size);
        if (video_data->size < lcd->info.width * lcd->info.height * 2) {
            return ESP_MEDIA_ERR_INVALID_ARG;
        }
        if (lcd->frame_buffer[0]) {
            // Check whether input ptr is frame buffer or not
            lcd->sel = !lcd->sel;
            return esp_lcd_panel_draw_bitmap(lcd->handle, 0, 0, lcd->info.width, lcd->info.height, rgb_data);
        }
#if CONFIG_IDF_TARGET_ESP32P4
        return esp_lcd_panel_draw_bitmap(lcd->handle, 0, 0, lcd->info.width, lcd->info.height, rgb_data);
#endif
        int w = lcd->info.width;
        int h = lcd->info.height;
        int i = 0;
        int n = 40;
        int ret = 0;
        while (i < h) {
            if (i + n > h) {
                n = h - i;
            }
            ret = esp_lcd_panel_draw_bitmap(lcd->handle, 0, i, w, i + n, rgb_data);
            rgb_data += n * w * 2;
            i += n;
        }
        return ret;
    }
    return ESP_MEDIA_ERR_NOT_SUPPORT;
}

static int lcd_render_get_frame_buffer(video_render_handle_t h, av_render_frame_buffer_t *buffer)
{
    lcd_render_t *lcd = (lcd_render_t *)h;
    if (lcd == NULL) {
        return ESP_MEDIA_ERR_INVALID_ARG;
    }
    if (lcd->frame_buffer[0] == NULL) {
        return ESP_MEDIA_ERR_NOT_SUPPORT;
    }
    uint8_t *frame_buffer = lcd->frame_buffer[lcd->sel];
    if (frame_buffer == NULL) {
        frame_buffer = lcd->frame_buffer[0];
    }
    // Only support RGB565 currently
    buffer->data = frame_buffer;
    buffer->size = lcd->info.width * lcd->info.height * 2;
    return 0;
}

static int lcd_render_get_latency(video_render_handle_t h, uint32_t *latency)
{
    lcd_render_t *lcd = (lcd_render_t *)h;
    if (lcd == NULL) {
        return ESP_MEDIA_ERR_INVALID_ARG;
    }
    *latency = 0;
    return 0;
}

static int lcd_render_get_frame_info(video_render_handle_t h, av_render_video_frame_info_t *info)
{
    lcd_render_t *lcd = (lcd_render_t *)h;
    if (lcd == NULL || info == NULL) {
        return ESP_MEDIA_ERR_INVALID_ARG;
    }
    memcpy(info, &lcd->info, sizeof(av_render_video_frame_info_t));
    return 0;
}

static int lcd_render_clear(video_render_handle_t h)
{
    lcd_render_t *lcd = (lcd_render_t *)h;
    if (lcd == NULL) {
        return ESP_MEDIA_ERR_INVALID_ARG;
    }
    if (lcd->handle && lcd->info.width) {
        uint8_t *frame_buffer = lcd->frame_buffer[lcd->sel];
        if (frame_buffer == NULL) {
#ifndef CONFIG_IDF_TARGET_ESP32P4
            uint8_t *line_buffer = (uint8_t *)media_lib_malloc(lcd->info.width * 2);
            if (line_buffer) {
                memset(line_buffer, 0, lcd->info.width * 2);
                for (int i = 0; i < lcd->info.height; i++) {
                    esp_lcd_panel_draw_bitmap(lcd->handle, 0, i, lcd->info.width, i + 1, line_buffer);
                }
                media_lib_free(line_buffer);
            }
#endif
        } else {
            int len = lcd->info.width * lcd->info.height * 2;
            memset(frame_buffer, 0, len);
            esp_lcd_panel_draw_bitmap(lcd->handle, 0, 0, 1, 1, frame_buffer);
        }
    }
    return 0;
}

static int lcd_render_close(video_render_handle_t h)
{
    lcd_render_t *lcd = (lcd_render_t *)h;
    if (lcd == NULL) {
        return ESP_MEDIA_ERR_INVALID_ARG;
    }
    lcd_render_clear(h);
    media_lib_free(lcd);
    return 0;
}

video_render_handle_t av_render_alloc_lcd_render(lcd_render_cfg_t *lcd_cfg)
{
    video_render_cfg_t cfg = {
        .ops = {
            .open = lcd_render_open,
            .format_support = lcd_render_format_supported,
            .get_frame_buffer = lcd_render_get_frame_buffer,
            .set_frame_info = lcd_render_set_frame_info,
            .write = lcd_render_write,
            .get_latency = lcd_render_get_latency,
            .get_frame_info = lcd_render_get_frame_info,
            .clear = lcd_render_clear,
            .close = lcd_render_close,
        },
        .cfg = lcd_cfg,
        .cfg_size = sizeof(lcd_render_cfg_t),
    };
    return video_render_alloc_handle(&cfg);
}
