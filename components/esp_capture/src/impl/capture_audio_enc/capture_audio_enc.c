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

#include <stdlib.h>
#include "esp_audio_enc_default.h"
#include "esp_capture_types.h"
#include "esp_capture_aenc_if.h"
#include "esp_log.h"

#define TAG "CAPTURE_AENC"

typedef struct {
    esp_capture_aenc_if_t    base;
    bool                     open;
    bool                     started;
    esp_capture_audio_info_t info;
    int                      bitrate;
    int                      in_frame_size;
    int                      out_frame_size;
    esp_audio_enc_handle_t   aenc_handle;
} general_aenc_t;

typedef union {
    esp_aac_enc_config_t   aac_cfg;
    esp_alac_enc_config_t  alac_cfg;
    esp_adpcm_enc_config_t adpcm_cfg;
    esp_amrnb_enc_config_t amrnb_cfg;
    esp_amrwb_enc_config_t amrwb_cfg;
    esp_g711_enc_config_t  g711_cfg;
    esp_opus_enc_config_t  opus_cfg;
} enc_all_cfg_t;

#define ASSIGN_BASIC_CFG(cfg) {                    \
    cfg->sample_rate     = info->sample_rate;      \
    cfg->bits_per_sample = info->bits_per_sample;  \
    cfg->channel         = info->channel;          \
}

static int get_encoder_config(esp_audio_enc_config_t *enc_cfg, esp_capture_audio_info_t *info)
{
    enc_all_cfg_t *all_cfg = (enc_all_cfg_t *)(enc_cfg->cfg);
    switch (info->codec) {
        case ESP_CAPTURE_CODEC_TYPE_AAC: {
            esp_aac_enc_config_t *cfg = &all_cfg->aac_cfg;
            ASSIGN_BASIC_CFG(cfg);
            enc_cfg->cfg_sz = sizeof(esp_aac_enc_config_t);
            cfg->bitrate = 90000;
            cfg->adts_used = true;
            break;
        }
        case ESP_CAPTURE_CODEC_TYPE_G711A:
        case ESP_CAPTURE_CODEC_TYPE_G711U: {
            esp_g711_enc_config_t *cfg = &all_cfg->g711_cfg;
            enc_cfg->cfg_sz = sizeof(esp_g711_enc_config_t);
            ASSIGN_BASIC_CFG(cfg);
            break;
        }
        case ESP_CAPTURE_CODEC_TYPE_OPUS: {
            esp_opus_enc_config_t *cfg = &all_cfg->opus_cfg;
            ASSIGN_BASIC_CFG(cfg);
            enc_cfg->cfg_sz = sizeof(esp_opus_enc_config_t);
            cfg->bitrate = 90000;
            cfg->frame_duration = ESP_OPUS_ENC_FRAME_DURATION_20_MS;
            cfg->application_mode = ESP_OPUS_ENC_APPLICATION_AUDIO;
            break;
        }
        default:
            ESP_LOGE(TAG, "Not supported encoder type %d", info->codec);
            return -1;
    }
    return 0;
}

static esp_audio_type_t get_audio_codec_type(esp_capture_codec_type_t codec)
{
    switch (codec) {
        case ESP_CAPTURE_CODEC_TYPE_AAC:
            return ESP_AUDIO_TYPE_AAC;
        case ESP_CAPTURE_CODEC_TYPE_G711A:
            return ESP_AUDIO_TYPE_G711A;
        case ESP_CAPTURE_CODEC_TYPE_G711U:
            return ESP_AUDIO_TYPE_G711U;
        case ESP_CAPTURE_CODEC_TYPE_OPUS:
            return ESP_AUDIO_TYPE_OPUS;
        default:
            return ESP_AUDIO_TYPE_UNSUPPORT;
    }
}

static int general_aenc_get_support_codecs(esp_capture_aenc_if_t *h, const esp_capture_codec_type_t **codecs, uint8_t *num)
{
    static esp_capture_codec_type_t acodecs[] = {
        ESP_CAPTURE_CODEC_TYPE_AAC,
        ESP_CAPTURE_CODEC_TYPE_G711A,
        ESP_CAPTURE_CODEC_TYPE_G711U,
        ESP_CAPTURE_CODEC_TYPE_OPUS,
    };
    *codecs = acodecs;
    *num = sizeof(acodecs) / sizeof(acodecs[0]);
    return ESP_CAPTURE_ERR_OK;
}

static bool is_aenc_supported(esp_capture_aenc_if_t *h, esp_capture_audio_info_t *info)
{
    const esp_capture_codec_type_t *codec = NULL;
    uint8_t num = 0;
    general_aenc_get_support_codecs(h, &codec, &num);
    for (int i = 0; i < num; i++) {
        if (codec[i] == info->codec) {
            return true;
        }
    }
    return false;
}

static int general_aenc_get_frame_size(esp_capture_aenc_if_t *h, int *in_frame_size, int *out_frame_size)
{
    general_aenc_t *aenc = (general_aenc_t *)h;
    if (aenc == NULL) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    if (aenc->started == false) {
        return ESP_CAPTURE_ERR_INVALID_STATE;
    }
    switch (aenc->info.codec) {
        case ESP_CAPTURE_CODEC_TYPE_G711A:
        case ESP_CAPTURE_CODEC_TYPE_G711U: {
            int samples = 20 * aenc->info.sample_rate / 1000;
            *in_frame_size = samples * (aenc->info.channel * aenc->info.bits_per_sample >> 3);
            *out_frame_size = *in_frame_size;
            break;
        }
        case ESP_CAPTURE_CODEC_TYPE_AAC:
        case ESP_CAPTURE_CODEC_TYPE_OPUS:
            esp_audio_enc_get_frame_size(aenc->aenc_handle, in_frame_size, out_frame_size);
            break;
        default:
            return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    return ESP_CAPTURE_ERR_OK;
}

static int general_aenc_start(esp_capture_aenc_if_t *h, esp_capture_audio_info_t *info)
{
    general_aenc_t *aenc = (general_aenc_t *)h;
    if (aenc == NULL) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    if (!is_aenc_supported(h, info)) {
        ESP_LOGE(TAG, "codec %d not supported", info->codec);
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    aenc->info = *info;
    enc_all_cfg_t all_cfg = { 0 };
    esp_audio_enc_config_t enc_cfg = {
        .type = get_audio_codec_type(info->codec),
        .cfg = &all_cfg,
    };
    // Get encoder configuration
    if (get_encoder_config(&enc_cfg, info) != 0) {
        ESP_LOGE(TAG, "Fail to get encoder config");
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    // Open encoder
    int ret = esp_audio_enc_open(&enc_cfg, &aenc->aenc_handle);
    if (ret != ESP_AUDIO_ERR_OK) {
        ESP_LOGE(TAG, "Fail to open encoder ret: %d", ret);
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    aenc->started = true;
    general_aenc_get_frame_size(h, &aenc->in_frame_size, &aenc->out_frame_size);
    ESP_LOGD(TAG, "%p Get frame size in:%d out:%d", h, aenc->in_frame_size, aenc->out_frame_size);
    return ESP_CAPTURE_ERR_OK;
}

static int general_aenc_set_bitrate(esp_capture_aenc_if_t *h, int bitrate)
{
    general_aenc_t *aenc = (general_aenc_t *)h;
    if (aenc == NULL) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    aenc->bitrate = bitrate;
    return ESP_CAPTURE_ERR_OK;
}

static int general_aenc_encode_frame(esp_capture_aenc_if_t *h, esp_capture_stream_frame_t *raw, esp_capture_stream_frame_t *encoded)
{
    general_aenc_t *aenc = (general_aenc_t *)h;
    if (aenc == NULL) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    if (aenc->started == false) {
        return ESP_CAPTURE_ERR_INVALID_STATE;
    }
    if (raw->size != aenc->in_frame_size || encoded->size < aenc->out_frame_size) {
        ESP_LOGE(TAG, "Bad frame size need in:%d vs %d  need out:%d but %d",
                 aenc->in_frame_size, raw->size, aenc->out_frame_size, encoded->size);
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    esp_audio_enc_in_frame_t in_frame = {
        .buffer = raw->data,
        .len = raw->size,
    };
    esp_audio_enc_out_frame_t out_frame = {
        .buffer = encoded->data,
        .len = encoded->size,
    };
    int ret = esp_audio_enc_process(aenc->aenc_handle, &in_frame, &out_frame);
    if (ret != ESP_AUDIO_ERR_OK) {
        ESP_LOGE(TAG, "Fail to encode audio frame return %d", ret);
        return ESP_CAPTURE_ERR_INTERNAL;
    }
    encoded->size = out_frame.encoded_bytes;
    return ESP_CAPTURE_ERR_OK;
}

static int general_aenc_stop(esp_capture_aenc_if_t *h)
{
    general_aenc_t *aenc = (general_aenc_t *)h;
    if (aenc == NULL) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    if (aenc->started == false) {
        ESP_LOGE(TAG, "Aenc already stopped");
        return ESP_CAPTURE_ERR_OK;
    }
    if (aenc->aenc_handle) {
        esp_audio_enc_close(aenc->aenc_handle);
        aenc->aenc_handle = NULL;
    }
    aenc->started = false;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_aenc_if_t *general_aenc_clone(esp_capture_aenc_if_t *aenc_if)
{
    general_aenc_t *aenc = (general_aenc_t *)calloc(1, sizeof(general_aenc_t));
    if (aenc == NULL) {
        return NULL;
    }
    aenc->base = ((general_aenc_t *)aenc_if)->base;
    return &aenc->base;
}

esp_capture_aenc_if_t *esp_capture_new_audio_encoder(void)
{
    general_aenc_t *aenc = (general_aenc_t *)calloc(1, sizeof(general_aenc_t));
    if (aenc == NULL) {
        return NULL;
    }
    aenc->base.clone = general_aenc_clone;
    aenc->base.get_support_codecs = general_aenc_get_support_codecs;
    aenc->base.start = general_aenc_start;
    aenc->base.get_frame_size = general_aenc_get_frame_size;
    aenc->base.set_bitrate = general_aenc_set_bitrate;
    aenc->base.encode_frame = general_aenc_encode_frame;
    aenc->base.stop = general_aenc_stop;
    return &aenc->base;
}
