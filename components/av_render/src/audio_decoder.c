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
#include "audio_decoder.h"
#include "esp_audio_dec.h"
#include "esp_audio_dec_default.h"
#include "media_lib_os.h"
#include "esp_log.h"

#define TAG "AUDIO_DEC"

#define ADEC_DEFAULT_OUTPUT_SIZE (4096)

typedef struct {
    av_render_audio_codec_t      codec;
    av_render_audio_frame_info_t frame_info;
    adec_frame_cb                frame_cb;
    esp_audio_dec_handle_t       dec_handle;
    bool                         header_parsed;
    void                        *ctx;
    uint8_t                     *frame_data;
    int                          frame_size;
} adec_t;

static esp_audio_type_t get_audio_decoder_type(av_render_audio_codec_t audio_format)
{
    switch (audio_format) {
        case AV_RENDER_AUDIO_CODEC_MP3:
            return ESP_AUDIO_TYPE_MP3;
        case AV_RENDER_AUDIO_CODEC_AAC:
            return ESP_AUDIO_TYPE_AAC;
        case AV_RENDER_AUDIO_CODEC_AMRNB:
            return ESP_AUDIO_TYPE_AMRNB;
        case AV_RENDER_AUDIO_CODEC_AMRWB:
            return ESP_AUDIO_TYPE_AMRWB;
        case AV_RENDER_AUDIO_CODEC_OPUS:
            return ESP_AUDIO_TYPE_OPUS;
        case AV_RENDER_AUDIO_CODEC_FLAC:
            return ESP_AUDIO_TYPE_FLAC;
        case AV_RENDER_AUDIO_CODEC_VORBIS:
            return ESP_AUDIO_TYPE_VORBIS;
        case AV_RENDER_AUDIO_CODEC_G711A:
            return ESP_AUDIO_TYPE_G711A;
        case AV_RENDER_AUDIO_CODEC_G711U:
            return ESP_AUDIO_TYPE_G711U;
        case AV_RENDER_AUDIO_CODEC_ADPCM:
            return ESP_AUDIO_TYPE_ADPCM;
        case AV_RENDER_AUDIO_CODEC_ALAC:
            return ESP_AUDIO_TYPE_ALAC;
        default:
            return ESP_AUDIO_TYPE_UNSUPPORT;
    }
}

static int _open_audio_dec(adec_t *adec, av_render_audio_info_t *stream_info)
{
    esp_audio_dec_cfg_t dec_cfg = {
        .type = get_audio_decoder_type(stream_info->codec),
    };
    if (dec_cfg.type == ESP_AUDIO_TYPE_UNSUPPORT) {
        return -1;
    }
    uint8_t *frame_data = (uint8_t *)media_lib_realloc(adec->frame_data, ADEC_DEFAULT_OUTPUT_SIZE);
    if (frame_data == NULL) {
        return ESP_MEDIA_ERR_NO_MEM;
    }
    adec->frame_size = ADEC_DEFAULT_OUTPUT_SIZE;
    adec->frame_data = frame_data;

    switch (dec_cfg.type) {
        case ESP_AUDIO_TYPE_VORBIS: {
#if 0
            ogg_vorbis_spec_info_t* vorbis_info = (ogg_vorbis_spec_info_t*)stream_info->codec_spec_info;
            esp_vorbis_dec_cfg_t vorbis_cfg = {
                .setup_header = vorbis_info->setup_header,
                .setup_size = vorbis_info->setup_size,
                .info_header = vorbis_info->info_header,
                .info_size = vorbis_info->info_size,
            };
            dec_cfg.cfg = &vorbis_cfg;
            dec_cfg.cfg_sz = sizeof(esp_vorbis_dec_cfg_t);
            esp_audio_dec_open(&dec_cfg, &adec->dec_handle);
#endif
        } break;
        case ESP_AUDIO_TYPE_OPUS: {
            esp_opus_dec_cfg_t opus_cfg = {
                .sample_rate = stream_info->sample_rate,
                .channel = stream_info->channel,
            };
            dec_cfg.cfg = &opus_cfg;
            dec_cfg.cfg_sz = sizeof(esp_opus_dec_cfg_t);
            esp_audio_dec_open(&dec_cfg, &adec->dec_handle);
        } break;
        case ESP_AUDIO_TYPE_ADPCM: {
            esp_adpcm_dec_cfg_t adpcm_cfg = {
                .sample_rate = stream_info->sample_rate,
                .channel = stream_info->channel,
                .bits_per_sample = stream_info->bits_per_sample,
            };
            dec_cfg.cfg = &adpcm_cfg;
            dec_cfg.cfg_sz = sizeof(esp_adpcm_dec_cfg_t);
            esp_audio_dec_open(&dec_cfg, &adec->dec_handle);
        } break;
        case ESP_AUDIO_TYPE_G711A:
        case ESP_AUDIO_TYPE_G711U: {
            esp_g711_dec_cfg_t g711_cfg = {
                .channel = stream_info->channel,
            };
            if (stream_info->channel) {
                dec_cfg.cfg = &g711_cfg;
                dec_cfg.cfg_sz = sizeof(esp_g711_dec_cfg_t);
            }
            esp_audio_dec_open(&dec_cfg, &adec->dec_handle);
        } break;
        case ESP_AUDIO_TYPE_ALAC: {
            esp_alac_dec_cfg_t alac_cfg = {
                .codec_spec_info = stream_info->codec_spec_info,
                .spec_info_len = stream_info->spec_info_len,
            };
            dec_cfg.cfg = &alac_cfg;
            dec_cfg.cfg_sz = sizeof(esp_alac_dec_cfg_t);
            esp_audio_dec_open(&dec_cfg, &adec->dec_handle);
        } break;
         case ESP_AUDIO_TYPE_AAC: {
            esp_aac_dec_cfg_t aac_cfg = {
                .channel = stream_info->channel,
                .sample_rate = stream_info->sample_rate,
                .bits_per_sample = stream_info->bits_per_sample,
                .no_adts_header = stream_info->aac_no_adts,
            };
            if (stream_info->aac_no_adts) {
                dec_cfg.cfg = &aac_cfg;
                dec_cfg.cfg_sz = sizeof(esp_alac_dec_cfg_t);
            }
            esp_audio_dec_open(&dec_cfg, &adec->dec_handle);
        } break;
        default:
            esp_audio_dec_open(&dec_cfg, &adec->dec_handle);
            break;
    }
    if (adec->dec_handle == NULL) {
        return -1;
    }
    return 0;
}

static int _close_audio_dec(adec_t *adec)
{
    if (adec->dec_handle) {
        esp_audio_dec_close(adec->dec_handle);
        adec->dec_handle = NULL;
        return 0;
    }
    return -1;
}

static int decoder_one_frame(adec_t *adec, uint8_t *data, int size, av_render_audio_frame_t *frame_data)
{
    esp_audio_dec_in_raw_t raw = {
        .buffer = data,
        .len = size,
    };
    esp_audio_dec_out_frame_t frame = {
        .buffer = adec->frame_data,
        .len = adec->frame_size,
    };
RETRY:
    frame.decoded_size = 0;
    esp_audio_err_t ret = esp_audio_dec_process(adec->dec_handle, &raw, &frame);
    if (ret == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
        ESP_LOGI(TAG, "Enlarge PCM buffer to %" PRIu32, frame.needed_size);
        uint8_t *output_fifo = (uint8_t *)media_lib_realloc(adec->frame_data, frame.needed_size);
        if (output_fifo == NULL) {
            return ESP_MEDIA_ERR_NO_MEM;
        }
        frame.buffer = output_fifo;
        frame.len = frame.needed_size;
        adec->frame_data = output_fifo;
        adec->frame_size = frame.needed_size;
        goto RETRY;
    }
    if (ret != ESP_AUDIO_ERR_OK) {
        ESP_LOGE(TAG, "Audio decode error %d", ret);
        return ret;
    }
    if (adec->header_parsed == false && frame.decoded_size > 0) {
        esp_audio_dec_info_t header = {};
        esp_audio_dec_get_info(adec->dec_handle, &header);
        printf("Get samplerate %d chanel %d\n", (int)header.sample_rate, header.channel);
        if (header.sample_rate && header.channel && header.bits_per_sample) {
            adec->frame_info.sample_rate = header.sample_rate;
            adec->frame_info.channel = header.channel;
            adec->frame_info.bits_per_sample = header.bits_per_sample;
        }
        adec->header_parsed = true;
    }
    frame_data->data = adec->frame_data;
    frame_data->size = frame.decoded_size;
    if (adec->frame_cb) {
        adec->frame_cb(frame_data, adec->ctx);
    }
    if (raw.consumed < raw.len) {
        raw.buffer += raw.consumed;
        raw.len -= raw.consumed;
        raw.consumed = 0;
        goto RETRY;
    }
    return ESP_MEDIA_ERR_OK;
}

static int _start_audio_dec(adec_t *adec, av_render_audio_data_t *frame, av_render_audio_frame_t *frame_data)
{
    if (frame->size == 0) {
        return ESP_MEDIA_ERR_INVALID_ARG;
    }
    uint8_t *dec_buffer = frame->data;
    frame_data->pts = frame->pts;
    return decoder_one_frame(adec, dec_buffer, frame->size, frame_data);
}

adec_handle_t adec_open(adec_cfg_t *cfg)
{
    adec_t *adec = (adec_t *)media_lib_calloc(1, sizeof(adec_t));
    if (adec == NULL) {
        return NULL;
    }
    adec->frame_cb = cfg->frame_cb;
    adec->ctx = cfg->ctx;
    adec->codec = cfg->audio_info.codec;
    adec->frame_info.sample_rate = cfg->audio_info.sample_rate;
    adec->frame_info.channel = cfg->audio_info.channel;
    adec->frame_info.bits_per_sample = cfg->audio_info.bits_per_sample;
    int ret = _open_audio_dec(adec, &cfg->audio_info);
    if (ret == 0) {
        return adec;
    }
    adec_close(adec);
    return NULL;
}

int adec_decode(adec_handle_t h, av_render_audio_data_t *data)
{
    if (h == NULL || data == NULL || (data->size == 0 && data->eos == false)) {
        return ESP_MEDIA_ERR_INVALID_ARG;
    }
    adec_t *adec = (adec_t *)h;
    av_render_audio_frame_t frame_data = {
        .pts = data->pts,
        .eos = data->eos,
    };
    if (data->size == 0 && data->eos) {
        // Not decode and send EOS directly
        if (adec->frame_cb) {
            adec->frame_cb(&frame_data, adec->ctx);
        }
        return ESP_MEDIA_ERR_OK;
    }
    return _start_audio_dec(adec, data, &frame_data);
}

int adec_get_frame_info(adec_handle_t h, av_render_audio_frame_info_t *frame_info)
{
    if (h == NULL || frame_info == NULL) {
        return ESP_MEDIA_ERR_INVALID_ARG;
    }
    adec_t *adec = (adec_t *)h;
    memcpy(frame_info, &adec->frame_info, sizeof(av_render_audio_frame_info_t));
    return 0;
}

int adec_close(adec_handle_t h)
{
    if (h == NULL) {
        return ESP_MEDIA_ERR_INVALID_ARG;
    }
    adec_t *adec = (adec_t *)h;
    _close_audio_dec(adec);
    if (adec->frame_data) {
        media_lib_free(adec->frame_data);
    }
    media_lib_free(adec);
    return 0;
}
