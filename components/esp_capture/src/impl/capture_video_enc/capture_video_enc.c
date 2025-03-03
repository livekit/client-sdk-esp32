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
#include "esp_capture_venc_if.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_video_enc.h"
#include "esp_video_codec_utils.h"

#define TAG "VENC"

#define ALIGN_UP(size, align) (((size) + (align)-1) & ~((align)-1))

typedef struct {
    esp_capture_venc_if_t       base;
    esp_capture_video_info_t    info;
    esp_video_codec_pixel_fmt_t src_fmt;
    int                         out_frame_size;
    int                         bitrate;
    bool                        started;
    esp_video_enc_handle_t      enc_handle;
} venc_inst_t;

static int venc_get_support_codecs(esp_capture_venc_if_t *h, const esp_capture_codec_type_t **codecs, uint8_t *num)
{
    static esp_capture_codec_type_t vcodecs[] = {
        ESP_CAPTURE_CODEC_TYPE_MJPEG,
        ESP_CAPTURE_CODEC_TYPE_H264,
    };
    *codecs = vcodecs;
    *num = sizeof(vcodecs) / sizeof(vcodecs[0]);
    return ESP_CAPTURE_ERR_OK;
}

static int venc_get_input_codecs(esp_capture_venc_if_t *h, esp_capture_codec_type_t out_codec, const esp_capture_codec_type_t **codecs, uint8_t *num)
{
    static esp_capture_codec_type_t jpeg_inputs[] = {
        ESP_CAPTURE_CODEC_TYPE_RGB565,
    };
    static esp_capture_codec_type_t h264_inputs[] = {
        ESP_CAPTURE_CODEC_TYPE_YUV420,
    };
    if (out_codec == ESP_CAPTURE_CODEC_TYPE_MJPEG) {
        *codecs = jpeg_inputs;
        *num = sizeof(jpeg_inputs) / sizeof(jpeg_inputs[0]);
        return ESP_CAPTURE_ERR_OK;
    }
    if (out_codec == ESP_CAPTURE_CODEC_TYPE_H264) {
        h264_inputs[0] = ESP_CAPTURE_CODEC_TYPE_YUV420;
        *codecs = h264_inputs;
        *num = sizeof(h264_inputs) / sizeof(h264_inputs[0]);
        return ESP_CAPTURE_ERR_OK;
    }
    *codecs = NULL;
    *num = 0;
    return ESP_CAPTURE_ERR_NOT_SUPPORTED;
}

static bool is_venc_codec_support(esp_capture_venc_if_t *h, esp_capture_codec_type_t out_codec, esp_capture_codec_type_t src_codec)
{
    const esp_capture_codec_type_t *in_codecs = NULL;
    uint8_t num = 0;
    venc_get_input_codecs(h, out_codec, &in_codecs, &num);
    for (int i = 0; i < num; i++) {
        if (in_codecs[i] == src_codec) {
            return true;
        }
    }
    return false;
}

static int venc_get_frame_size(esp_capture_venc_if_t *h, int *in_frame_size, int *out_frame_size)
{
    venc_inst_t *venc = (venc_inst_t *)h;
    if (venc->started == false) {
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    esp_video_codec_resolution_t res = {
        .width = venc->info.width,
        .height = venc->info.height,
    };
    *in_frame_size = (int)esp_video_codec_get_image_size(venc->src_fmt, &res);
    if (venc->info.codec == ESP_CAPTURE_CODEC_TYPE_MJPEG) {
        *out_frame_size = *in_frame_size / 20;
    } else if (venc->info.codec == ESP_CAPTURE_CODEC_TYPE_H264) {
        *out_frame_size = *in_frame_size / 2;
        *out_frame_size = ALIGN_UP(*out_frame_size, 128);
    } else {
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    return ESP_CAPTURE_ERR_OK;
}

static esp_video_codec_type_t map_codec_type(esp_capture_codec_type_t codec)
{
    switch (codec) {
        case ESP_CAPTURE_CODEC_TYPE_MJPEG:
            return ESP_VIDEO_CODEC_TYPE_MJPEG;
        case ESP_CAPTURE_CODEC_TYPE_H264:
            return ESP_VIDEO_CODEC_TYPE_H264;
        default:
            return ESP_VIDEO_CODEC_TYPE_NONE;
    }
}

static esp_video_codec_pixel_fmt_t map_pixel_fmt(esp_capture_codec_type_t codec)
{
    switch (codec) {
        case ESP_CAPTURE_CODEC_TYPE_RGB565:
            return ESP_VIDEO_CODEC_PIXEL_FMT_RGB565_LE;
        case ESP_CAPTURE_CODEC_TYPE_YUV420:
        #if CONFIG_IDF_TARGET_ESP32S3
            return ESP_VIDEO_CODEC_PIXEL_FMT_YUV420P;
        #endif
            return ESP_VIDEO_CODEC_PIXEL_FMT_O_UYY_E_VYY;
        default:
            return ESP_VIDEO_CODEC_PIXEL_FMT_NONE;
    }
}

static int venc_start(esp_capture_venc_if_t *h, esp_capture_codec_type_t src_codec, esp_capture_video_info_t *info)
{
    venc_inst_t *venc = (venc_inst_t *)h;
    if (is_venc_codec_support(h, info->codec, src_codec) == false) {
        ESP_LOGE(TAG, "Codec not supported src:%d dst:%d", src_codec, info->codec);
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    venc->info = *info;
    venc->src_fmt = map_pixel_fmt(src_codec);
    esp_video_enc_cfg_t enc_cfg = {
        .codec_type = map_codec_type(info->codec),
        .resolution = {
            .width = info->width,
            .height = info->height,
        },
        .in_fmt = map_pixel_fmt(src_codec),
        .fps = info->fps,
    };
    int ret = esp_video_enc_open(&enc_cfg, &venc->enc_handle);
    if (ret != ESP_VC_ERR_OK) {
        ESP_LOGE(TAG, "Fail to open encoder");
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    venc->started = true;
    int in_size = 0;
    venc_get_frame_size(h, &in_size, &venc->out_frame_size);
    return ESP_CAPTURE_ERR_OK;
}

static int venc_set_bitrate(esp_capture_venc_if_t *h, int bitrate)
{
    venc_inst_t *venc = (venc_inst_t *)h;
    esp_video_enc_set_bitrate(venc->enc_handle, bitrate);
    return ESP_CAPTURE_ERR_OK;
}

static int venc_encode_frame(esp_capture_venc_if_t *h, esp_capture_stream_frame_t *raw, esp_capture_stream_frame_t *encoded)
{
    venc_inst_t *venc = (venc_inst_t *)h;
    if (venc->started == false || venc->enc_handle == NULL) {
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    if (encoded->size < venc->out_frame_size) {
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    esp_video_enc_in_frame_t in_frame = {
        .pts = raw->pts,
        .data = raw->data,
        .size = raw->size,
    };
    esp_video_enc_out_frame_t out_frame = {
        .data = encoded->data,
        .size = encoded->size,
    };
    int ret = esp_video_enc_process(venc->enc_handle, &in_frame, &out_frame);
    if (ret == ESP_VC_ERR_BUF_NOT_ENOUGH) {
        return ESP_CAPTURE_ERR_NOT_ENOUGH;
    }
    if (ret != ESP_VC_ERR_OK) {
        ESP_LOGE(TAG, "Fail to encode frame");
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    encoded->pts = out_frame.pts;
    encoded->size = out_frame.encoded_size;
    ;
    return ESP_CAPTURE_ERR_OK;
}

static int venc_stop(esp_capture_venc_if_t *h)
{
    venc_inst_t *venc = (venc_inst_t *)h;
    if (venc->enc_handle) {
        esp_video_enc_close(venc->enc_handle);
        venc->enc_handle = NULL;
    }
    venc->started = false;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_venc_if_t *venc_clone(esp_capture_venc_if_t *h)
{
    venc_inst_t *venc = (venc_inst_t *)calloc(1, sizeof(venc_inst_t));
    if (venc == NULL) {
        return NULL;
    }
    venc_inst_t *his = (venc_inst_t *)h;
    venc->base = his->base;
    return &(venc->base);
}

esp_capture_venc_if_t *esp_capture_new_video_encoder(void)
{
    venc_inst_t *venc = (venc_inst_t *)calloc(1, sizeof(venc_inst_t));
    if (venc == NULL) {
        return NULL;
    }
    venc->base.clone = venc_clone;
    venc->base.get_support_codecs = venc_get_support_codecs;
    venc->base.get_input_codecs = venc_get_input_codecs;
    venc->base.set_bitrate = venc_set_bitrate;
    venc->base.start = venc_start;
    venc->base.get_frame_size = venc_get_frame_size;
    venc->base.encode_frame = venc_encode_frame;
    venc->base.stop = venc_stop;
    return &(venc->base);
}
