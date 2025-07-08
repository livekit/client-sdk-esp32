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

#include "esp_capture_types.h"
#include "esp_capture_audio_src_if.h"
#include "esp_capture_defaults.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include <string.h>

#define TAG "CODEC_DEV_SRC"

typedef struct {
    esp_capture_audio_src_if_t base;
    esp_codec_dev_handle_t     handle;
    esp_capture_audio_info_t   info;
    int                        frame_num;
    uint64_t                   frames;
    bool                       start;
    bool                       open;
} codec_dev_src_t;

static int codec_dev_src_open(esp_capture_audio_src_if_t *h)
{
    codec_dev_src_t *src = (codec_dev_src_t *)h;
    if (src->handle == NULL) {
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    src->frame_num = 0;
    src->open = true;
    return ESP_CAPTURE_ERR_OK;
}

static int codec_dev_src_get_support_codecs(esp_capture_audio_src_if_t *src, const esp_capture_codec_type_t **codecs, uint8_t *num)
{
    static esp_capture_codec_type_t support_codecs[] = { ESP_CAPTURE_CODEC_TYPE_PCM };
    *codecs = support_codecs;
    *num = 1;
    return ESP_CAPTURE_ERR_OK;
}

static int codec_dev_src_negotiate_caps(esp_capture_audio_src_if_t *h, esp_capture_audio_info_t *in_cap, esp_capture_audio_info_t *out_caps)
{
    codec_dev_src_t *src = (codec_dev_src_t *)h;
    const esp_capture_codec_type_t *codecs = NULL;
    uint8_t num = 0;
    codec_dev_src_get_support_codecs(h, &codecs, &num);
    for (int i = 0; i < num; i++) {
        if (codecs[i] == in_cap->codec) {
            memcpy(out_caps, in_cap, sizeof(esp_capture_audio_info_t));
            src->info = *in_cap;
            return ESP_CAPTURE_ERR_OK;
        }
    }
    return ESP_CAPTURE_ERR_NOT_SUPPORTED;
}

static int codec_dev_src_start(esp_capture_audio_src_if_t *h)
{
    codec_dev_src_t *src = (codec_dev_src_t *)h;
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = src->info.sample_rate,
        .bits_per_sample = src->info.bits_per_sample,
        .channel = src->info.channel,
    };
    int ret = esp_codec_dev_open(src->handle, &fs);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to open codec device, ret=%d", ret);
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    src->start = true;
    src->frame_num = 0;
    src->frames = 0;
    return ESP_CAPTURE_ERR_OK;
}

static int codec_dev_src_read_frame(esp_capture_audio_src_if_t *h, esp_capture_stream_frame_t *frame)
{
    codec_dev_src_t *src = (codec_dev_src_t *)h;
    if (src->start == false) {
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    int ret = esp_codec_dev_read(src->handle, frame->data, frame->size);
    if (ret == 0) {
        int samples = frame->size / (src->info.bits_per_sample / 8 * src->info.channel);
        frame->pts = src->frame_num * samples * 1000 / src->info.sample_rate;
        src->frame_num++;
    }
    return ret == 0 ? ESP_CAPTURE_ERR_OK : ESP_CAPTURE_ERR_INTERNAL;
}

static int codec_dev_src_stop(esp_capture_audio_src_if_t *h)
{
    codec_dev_src_t *src = (codec_dev_src_t *)h;
    if (src->handle) {
        esp_codec_dev_close(src->handle);
    }
    src->start = false;
    return ESP_CAPTURE_ERR_OK;
}

static int codec_dev_src_close(esp_capture_audio_src_if_t *h)
{
    codec_dev_src_t *src = (codec_dev_src_t *)h;
    src->handle = NULL;
    return ESP_CAPTURE_ERR_OK;
}

esp_capture_audio_src_if_t *esp_capture_new_audio_codec_src(esp_capture_audio_codec_src_cfg_t *cfg)
{
    if (cfg == NULL || cfg->record_handle == NULL) {
        return NULL;
    }
    codec_dev_src_t *src = calloc(1, sizeof(codec_dev_src_t));
    src->base.open = codec_dev_src_open;
    src->base.get_support_codecs = codec_dev_src_get_support_codecs;
    src->base.negotiate_caps = codec_dev_src_negotiate_caps;
    src->base.start = codec_dev_src_start;
    src->base.read_frame = codec_dev_src_read_frame;
    src->base.stop = codec_dev_src_stop;
    src->base.close = codec_dev_src_close;
    src->handle = cfg->record_handle;
    return &src->base;
}