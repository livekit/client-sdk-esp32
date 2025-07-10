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
#include <inttypes.h>
#include "av_render_default.h"
#include "media_lib_os.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "esp_ae_sonic.h"

#define TAG "I2S_RENDER"

#define SAMPLE_SIZE(info) (info.channel * info.bits_per_sample >> 3)

#define DEFALUT_SONIC_OUT_SAMPLES (1024 * 4)

typedef struct {
    audio_render_ref_cb          ref_cb;
    av_render_audio_frame_info_t info;
    void                        *ref_ctx;
    bool                         fixed_clock;
    esp_codec_dev_handle_t       play_handle;
    void                        *sonic_handle;
    bool                         sonic_enable;
    char                        *sonic_out;
    int                          sonic_out_size;
} i2s_render_t;

static audio_render_handle_t i2s_render_init(void *cfg, int cfg_size)
{
    i2s_render_cfg_t *i2s_cfg = (i2s_render_cfg_t *)cfg;
    if (cfg == NULL || cfg_size != sizeof(i2s_render_cfg_t) || i2s_cfg->play_handle == NULL) {
        return NULL;
    }
    i2s_render_t *i2s = (i2s_render_t *)media_lib_calloc(1, sizeof(i2s_render_t));
    if (i2s == NULL) {
        return NULL;
    }
    i2s->ref_cb = i2s_cfg->cb;
    i2s->ref_ctx = i2s_cfg->ctx;
    i2s->fixed_clock = i2s_cfg->fixed_clock;
    i2s->play_handle = i2s_cfg->play_handle;
    if (i2s->play_handle == NULL) {
        ESP_LOGE(TAG, "Play device not exists");
        media_lib_free(i2s);
        return NULL;
    }
    return i2s;
}

static int i2s_render_open(audio_render_handle_t render, av_render_audio_frame_info_t *info)
{
    if (render == NULL || info == NULL) {
        return -1;
    }
    i2s_render_t *i2s = (i2s_render_t *)render;
    // TODO, notes when set clk i2s should not in reading or writing status or else it wil hangup
    ESP_LOGI(TAG, "open channel:%d sample_rate:%d bits:%d", info->channel, (int)info->sample_rate, info->bits_per_sample);
    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = info->bits_per_sample,
        .sample_rate = info->sample_rate,
        .channel = info->channel,
    };
    int ret = esp_codec_dev_open(i2s->play_handle, &fs);
    if (ret == 0) {
        memcpy(&i2s->info, info, sizeof(av_render_audio_frame_info_t));
    }
    return ret;
}

static int i2s_render_write(audio_render_handle_t render, av_render_audio_frame_t *audio_data)
{
    if (render == NULL || audio_data == NULL) {
        return -1;
    }
    int ret = ESP_OK;
    i2s_render_t *i2s = (i2s_render_t *)render;
    if (i2s->sonic_enable) {
        int sample_num = audio_data->size / SAMPLE_SIZE(i2s->info);
        esp_ae_sonic_in_data_t in_samples = {
            .num = sample_num,
            .samples = audio_data->data,
        };
        esp_ae_sonic_out_data_t out_samples = {
            .samples = i2s->sonic_out,
            .needed_num = DEFALUT_SONIC_OUT_SAMPLES,
        };
        while (sample_num > 0) {
            ret = esp_ae_sonic_process(i2s->sonic_handle, &in_samples, &out_samples);
            if (ret != ESP_AE_ERR_OK) {
                printf("sonic process error\n");
                return -1;
            }
            if (out_samples.out_num == 0) {
                break;
            }
            ret = esp_codec_dev_write(i2s->play_handle, out_samples.samples, out_samples.out_num * SAMPLE_SIZE(i2s->info));
            if (ret == 0 && i2s->ref_cb) {
                i2s->ref_cb(audio_data->data, audio_data->size, i2s->ref_ctx);
            }
            sample_num -= in_samples.consume_num;
            in_samples.num = sample_num;
            in_samples.samples = audio_data->data + in_samples.consume_num * SAMPLE_SIZE(i2s->info);
        }
    } else {
        int ret = esp_codec_dev_write(i2s->play_handle, audio_data->data, audio_data->size);
        if (ret == 0 && i2s->ref_cb) {
            i2s->ref_cb(audio_data->data, audio_data->size, i2s->ref_ctx);
        }
    }
    return ret;
}

static int i2s_render_get_latency(audio_render_handle_t render, uint32_t *latency)
{
    if (render == NULL) {
        return -1;
    }
    *latency = 0;
    return 0;
}

static int i2s_render_get_frame_info(audio_render_handle_t render, av_render_audio_frame_info_t *info)
{
    if (render == NULL || info == NULL) {
        return -1;
    }
    i2s_render_t *i2s = (i2s_render_t *)render;
    memcpy(info, &i2s->info, sizeof(av_render_audio_frame_info_t));
    return 0;
}

static int i2s_render_set_speed(audio_render_handle_t render, float speed)
{
    if (render == NULL) {
        return -1;
    }
    i2s_render_t *i2s = (i2s_render_t *)render;
    if (speed != 1.0) {
        if (i2s->sonic_handle == NULL) {
            esp_ae_sonic_cfg_t cfg = {
                .sample_rate = i2s->info.sample_rate,
                .channel = i2s->info.channel,
                .bits_per_sample = i2s->info.bits_per_sample,
            };
            esp_ae_sonic_open(&cfg, &i2s->sonic_handle);
            i2s->sonic_out = media_lib_malloc(DEFALUT_SONIC_OUT_SAMPLES * SAMPLE_SIZE(i2s->info));
        }
        if (i2s->sonic_handle && i2s->sonic_out) {
            i2s->sonic_enable = true;
        }
    }
    if (i2s->sonic_enable) {
        esp_ae_sonic_set_speed(i2s->sonic_handle, speed);
    }
    return 0;
}

static int i2s_render_close(audio_render_handle_t render)
{
    if (render == NULL) {
        return -1;
    }
    i2s_render_t *i2s = (i2s_render_t *)render;
    esp_codec_dev_close(i2s->play_handle);
    if (i2s->sonic_handle) {
        esp_ae_sonic_close(i2s->sonic_handle);
        i2s->sonic_handle = NULL;
    }
    if (i2s->sonic_out) {
        media_lib_free(i2s->sonic_out);
        i2s->sonic_out = NULL;
    }
    i2s->sonic_enable = false;
    return 0;
}

static void i2s_render_deinit(audio_render_handle_t render)
{
    if (render == NULL) {
        return;
    }
    media_lib_free(render);
}

audio_render_handle_t av_render_alloc_i2s_render(i2s_render_cfg_t *i2s_cfg)
{
    audio_render_cfg_t cfg = {
        .ops = {
            .init = i2s_render_init,
            .open = i2s_render_open,
            .write = i2s_render_write,
            .get_latency = i2s_render_get_latency,
            .set_speed = i2s_render_set_speed,
            .get_frame_info = i2s_render_get_frame_info,
            .close = i2s_render_close,
            .deinit = i2s_render_deinit,
        },
        .cfg = i2s_cfg,
        .cfg_size = sizeof(i2s_render_cfg_t),
    };
    return audio_render_alloc_handle(&cfg);
}
