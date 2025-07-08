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
#include <stdio.h>
#include <string.h>
#include "esp_log.h"

#define TAG "AUD_FILE_SRC"

typedef struct {
    esp_capture_audio_src_if_t base;
    esp_capture_audio_info_t   aud_info;
    char                       file_path[128];
    FILE                      *fp;
    bool                       is_open;
    bool                       is_start;
    bool                       nego_ok;
} aud_file_src_t;

static int get_aud_info_by_name(aud_file_src_t *src)
{
    char *ext = strrchr(src->file_path, '.');
    if (ext == NULL) {
        return -1;
    }
    if (strcmp(ext, ".pcm") == 0) {
        src->aud_info.codec = ESP_CAPTURE_CODEC_TYPE_PCM;
        src->aud_info.sample_rate = 44100;
        src->aud_info.bits_per_sample = 16;
        src->aud_info.channel = 2;
    } else if (strcmp(ext, ".opus") == 0) {
        src->aud_info.codec = ESP_CAPTURE_CODEC_TYPE_OPUS;
        src->aud_info.sample_rate = 44100;
        src->aud_info.bits_per_sample = 16;
        src->aud_info.channel = 2;
    } else {
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    return ESP_CAPTURE_ERR_OK;
}

static int aud_file_src_close(esp_capture_audio_src_if_t *h)
{
    aud_file_src_t *src = (aud_file_src_t *)h;
    if (src->fp) {
        fclose(src->fp);
        src->fp = NULL;
    }
    return 0;
}

static int aud_file_src_open(esp_capture_audio_src_if_t *h)
{
    aud_file_src_t *src = (aud_file_src_t *)h;
    src->fp = fopen(src->file_path, "rb");
    if (src->fp == NULL) {
        ESP_LOGE(TAG, "open file failed");
        return ESP_CAPTURE_ERR_NOT_FOUND;
    }
    int ret = get_aud_info_by_name(src);
    if (ret != ESP_CAPTURE_ERR_OK) {
        aud_file_src_close(h);
        return ret;
    }
    src->is_open = true;
    return 0;
}

static int aud_file_src_get_codec(esp_capture_audio_src_if_t *h, const esp_capture_codec_type_t **codecs, uint8_t *num)
{
    aud_file_src_t *src = (aud_file_src_t *)h;
    if (src->is_open == false) {
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    *num = 1;
    *codecs = &src->aud_info.codec;
    return ESP_CAPTURE_ERR_OK;
}

static int aud_file_src_nego(esp_capture_audio_src_if_t *h, esp_capture_audio_info_t *in_cap, esp_capture_audio_info_t *out_caps)
{
    aud_file_src_t *src = (aud_file_src_t *)h;
    if (src->is_open == false) {
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    if (in_cap->codec == src->aud_info.codec) {
        src->nego_ok = true;
        *out_caps = src->aud_info;
        return ESP_CAPTURE_ERR_OK;
    }
    return ESP_CAPTURE_ERR_NOT_SUPPORTED;
}

static int aud_file_src_start(esp_capture_audio_src_if_t *h)
{
    aud_file_src_t *src = (aud_file_src_t *)h;
    if (src->nego_ok == false) {
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    src->is_start = true;
    return ESP_CAPTURE_ERR_OK;
}

static int aud_file_src_read_frame(esp_capture_audio_src_if_t *h, esp_capture_stream_frame_t *frame)
{
    aud_file_src_t *src = (aud_file_src_t *)h;
    if (src->is_start == false) {
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    if (src->aud_info.codec == ESP_CAPTURE_CODEC_TYPE_PCM) {
        int ret = fread(frame->data, 1, frame->size, src->fp);
        if (ret >= 0) {
            frame->size = ret;
            return ESP_CAPTURE_ERR_OK;
        }
    } else if (src->aud_info.codec == ESP_CAPTURE_CODEC_TYPE_OPUS) {
        uint32_t payload_size = 0;
        int ret = fread(&payload_size, 1, 4, src->fp);
        if (payload_size && frame->size >= payload_size) {
            ret = fread(frame->data, 1, payload_size, src->fp);
            if (ret >= 0) {
                frame->size = ret;
                return ESP_CAPTURE_ERR_OK;
            }
        }
    }
    return ESP_CAPTURE_ERR_NOT_SUPPORTED;
}

static int aud_file_src_stop(esp_capture_audio_src_if_t *h)
{
    aud_file_src_t *src = (aud_file_src_t *)h;
    src->nego_ok = false;
    src->is_start = false;
    if (src->fp) {
        fseek(src->fp, 0, SEEK_SET);
    }
    return ESP_CAPTURE_ERR_OK;
}

esp_capture_audio_src_if_t *esp_capture_new_audio_file_src(const char *file_name)
{
    aud_file_src_t *src = (aud_file_src_t *)calloc(1, sizeof(aud_file_src_t));
    if (src == NULL) {
        return NULL;
    }
    strncpy(src->file_path, file_name, sizeof(src->file_path) - 1);
    src->base.open = aud_file_src_open;
    src->base.get_support_codecs = aud_file_src_get_codec;
    src->base.negotiate_caps = aud_file_src_nego;
    src->base.start = aud_file_src_start;
    src->base.read_frame = aud_file_src_read_frame;
    src->base.stop = aud_file_src_stop;
    src->base.close = aud_file_src_close;
    return &src->base;
}
