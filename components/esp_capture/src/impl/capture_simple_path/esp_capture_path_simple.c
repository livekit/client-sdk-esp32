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

#include <string.h>
#include <stdlib.h>
#include "esp_capture_path_simple.h"
#include "esp_log.h"
#include "media_lib_os.h"
#include "data_queue.h"

#define TAG "CAPTURE_SIMP"

#define CAPTURE_AENC_EXITED (1)
#define CAPTURE_VENC_EXITED (2)

#define VIDEO_ENC_OUT_ALIGNMENT (128)
#define ALIGN_UP(size, align)   (((size) + (align)-1) & ~((align)-1))

typedef struct {
    bool                         added;
    bool                         enable;
    bool                         started;
    bool                         venc_bypass;
    bool                         aenc_bypass;
    bool                         video_enabled;
    bool                         audio_enabled;
    esp_capture_codec_type_t     video_src_codec;
    esp_capture_sink_cfg_t       sink;
    data_queue_t                *audio_q;
    data_queue_t                *video_q;
    int                          audio_frame_size;
    int                          video_frame_size;
    media_lib_event_grp_handle_t event_group;
} simple_capture_res_t;

typedef struct {
    esp_capture_path_if_t         base;
    esp_capture_simple_path_cfg_t enc_cfg;
    esp_capture_path_cfg_t        src_cfg;
    esp_capture_audio_info_t      aud_info;
    esp_capture_audio_info_t      vid_info;
    simple_capture_res_t          primary;
} simple_capture_t;

int simple_capture_open(esp_capture_path_if_t *h, esp_capture_path_cfg_t *cfg)
{
    simple_capture_t *capture = (simple_capture_t *)h;
    if (cfg->acquire_src_frame == NULL || cfg->release_src_frame == NULL || (cfg->nego_audio == NULL && cfg->nego_video == NULL) || cfg->frame_processed == NULL) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    capture->src_cfg = *cfg;
    return ESP_CAPTURE_ERR_OK;
}

static bool need_bypass_aenc(simple_capture_t *capture, esp_capture_audio_info_t *audio_info)
{
    esp_capture_audio_info_t in_info = {};
    int ret = capture->src_cfg.nego_audio(capture->src_cfg.src_ctx, audio_info, &in_info);
    if (ret == ESP_CAPTURE_ERR_OK && in_info.codec == audio_info->codec) {
        ESP_LOGI(TAG, "Bypass audio encoder for codec %d", audio_info->codec);
        return true;
    }
    return false;
}

static bool need_bypass_venc(simple_capture_t *capture, esp_capture_video_info_t *video_info)
{
    esp_capture_video_info_t in_info = {};
    int ret = capture->src_cfg.nego_video(capture->src_cfg.src_ctx, video_info, &in_info);
    if (ret == ESP_CAPTURE_ERR_OK && in_info.codec == video_info->codec) {
        ESP_LOGI(TAG, "Bypass video encoder for codec %d", video_info->codec);
        return true;
    }
    return false;
}

static bool check_audio_codec_support(simple_capture_t *capture, esp_capture_aenc_if_t *aenc, esp_capture_audio_info_t *audio_info)
{
    if (aenc == NULL) {
        ESP_LOGE(TAG, "Not support audio encoder");
        return false;
    }
    if (need_bypass_aenc(capture, audio_info)) {
        capture->primary.aenc_bypass = true;
        return true;
    }
    const esp_capture_codec_type_t *acodecs = NULL;
    uint8_t num = 0;
    aenc->get_support_codecs(aenc, &acodecs, &num);
    for (int i = 0; i < num; i++) {
        if (acodecs[i] == audio_info->codec) {
            esp_capture_audio_info_t pcm_info;
            esp_capture_audio_info_t in_info;
            pcm_info = *audio_info;
            pcm_info.codec = ESP_CAPTURE_CODEC_TYPE_PCM;
            capture->src_cfg.nego_audio(capture->src_cfg.src_ctx, &pcm_info, &in_info);
            if (in_info.sample_rate != audio_info->sample_rate || in_info.channel != audio_info->channel || in_info.bits_per_sample != audio_info->bits_per_sample) {
                ESP_LOGE(TAG, "Sample rate or channel not supported by source");
                break;
            }
            return true;
        }
    }
    ESP_LOGE(TAG, "Audio encoder not support codec %d", audio_info->codec);
    return false;
}

static bool check_video_codec_support(simple_capture_t *capture, esp_capture_venc_if_t *venc, esp_capture_video_info_t *video_info)
{
    if (venc == NULL) {
        ESP_LOGE(TAG, "Not support video encoder");
        return false;
    }
    if (need_bypass_venc(capture, video_info)) {
        capture->primary.venc_bypass = true;
        return true;
    }
    const esp_capture_codec_type_t *vcodecs = NULL;
    uint8_t num = 0;
    venc->get_support_codecs(venc, &vcodecs, &num);
    for (int i = 0; i < num; i++) {
        printf("cmp %d %d\n", vcodecs[i], video_info->codec);
        if (vcodecs[i] == video_info->codec) {
            esp_capture_video_info_t in_info = *video_info;
            esp_capture_video_info_t out_info;
            const esp_capture_codec_type_t *in_codecs = NULL;
            num = 0;
            venc->get_input_codecs(venc, video_info->codec, &in_codecs, &num);
            for (i = 0; i < num; i++) {
                in_info.codec = in_codecs[i];
                printf("Source to nego %d\n", in_codecs[i]);
                int ret = capture->src_cfg.nego_video(capture->src_cfg.src_ctx, &in_info, &out_info);
                if (ret == ESP_CAPTURE_ERR_OK) {
                    if (in_info.width != out_info.width || in_info.height != out_info.height) {
                        ESP_LOGE(TAG, "Resolution not supported by source");
                        break;
                    }
                    capture->primary.video_src_codec = in_info.codec;
                    return true;
                }
            }
            break;
        }
    }
    ESP_LOGE(TAG, "video encoder not support codec %d", video_info->codec);
    return false;
}

int simple_capture_add_path(esp_capture_path_if_t *p, esp_capture_path_type_t path, esp_capture_sink_cfg_t *sink)
{
    simple_capture_t *capture = (simple_capture_t *)p;
    if (path != ESP_CAPTURE_PATH_PRIMARY) {
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    simple_capture_res_t *res = &capture->primary;
    if (res->added) {
        return ESP_CAPTURE_ERR_INVALID_STATE;
    }
    media_lib_event_group_create(&res->event_group);
    if (res->event_group == NULL) {
        return ESP_CAPTURE_ERR_NO_MEM;
    }
    res->sink = *sink;
    if (sink->audio_info.codec && check_audio_codec_support(capture, capture->enc_cfg.aenc, &sink->audio_info) == false) {
        res->sink.audio_info.codec = ESP_CAPTURE_CODEC_TYPE_NONE;
    }
    if (sink->video_info.codec && check_video_codec_support(capture, capture->enc_cfg.venc, &sink->video_info) == false) {
        res->sink.video_info.codec = ESP_CAPTURE_CODEC_TYPE_NONE;
    }
    res->added = true;
    return ESP_CAPTURE_ERR_OK;
}

static int get_frame_samples(esp_capture_audio_info_t *audio_info, int dur)
{
    return dur * audio_info->sample_rate / 1000;
}

int simple_capture_get_frame_samples(esp_capture_path_if_t *p, esp_capture_path_type_t path)
{
    simple_capture_t *capture = (simple_capture_t *)p;
    if (path != ESP_CAPTURE_PATH_PRIMARY) {
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    simple_capture_res_t *res = &capture->primary;
    if (res->sink.audio_info.codec == ESP_CAPTURE_CODEC_TYPE_NONE) {
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    esp_capture_audio_info_t *aud_info = &res->sink.audio_info;
    int frame_samples = get_frame_samples(aud_info, 20);
    if (res->aenc_bypass == false && res->audio_enabled) {
        int in_frame_size = 0, out_frame_size = 0;
        capture->enc_cfg.aenc->get_frame_size(capture->enc_cfg.aenc, &in_frame_size, &out_frame_size);
        if (in_frame_size) {
            int frame_size = aud_info->channel * aud_info->bits_per_sample >> 3;
            frame_samples = in_frame_size / frame_size;
        }
    }
    ESP_LOGI(TAG, "Set audio frame size %d", frame_samples);
    return frame_samples;
}

int simple_capture_add_overlay(esp_capture_path_if_t *h, esp_capture_path_type_t path, esp_capture_overlay_if_t *overlay)
{
    return ESP_CAPTURE_ERR_NOT_SUPPORTED;
}

int simple_capture_enable_overlay(esp_capture_path_if_t *p, esp_capture_path_type_t path, bool enable)
{
    return ESP_CAPTURE_ERR_NOT_SUPPORTED;
}

static void simple_capture_aenc_thread(void *arg)
{
    simple_capture_t *capture = (simple_capture_t *)arg;
    simple_capture_res_t *res = &capture->primary;
    esp_capture_stream_frame_t out_frame;
    out_frame.stream_type = ESP_CAPTURE_STREAM_TYPE_AUDIO;
    ESP_LOGI(TAG, "Enter audio encoder thread");
    while (res->audio_enabled) {
        // grab data from src
        esp_capture_stream_frame_t frame;
        frame.stream_type = ESP_CAPTURE_STREAM_TYPE_AUDIO;
        int ret = capture->src_cfg.acquire_src_frame(capture->src_cfg.src_ctx, &frame, false);
        if (ret != ESP_CAPTURE_ERR_OK) {
            ESP_LOGE(TAG, "Fail to acquire audio frame ret %d", ret);
            break;
        }
        // TODO use original frame is OK?
        if (res->aenc_bypass) {
            capture->src_cfg.frame_processed(capture->src_cfg.src_ctx, ESP_CAPTURE_PATH_PRIMARY, &frame);
            if (frame.data == NULL && frame.size == 0) {
                ESP_LOGI(TAG, "Stop frame is received");
                break;
            }
            continue;
        }
        int frame_size = res->audio_frame_size + sizeof(esp_capture_stream_frame_t);
        uint8_t *data = data_queue_get_buffer(res->audio_q, frame_size);
        if (data == NULL) {
            ESP_LOGE(TAG, "Fail to get audio fifo buffer");
            break;
        }
        out_frame.pts = frame.pts;
        out_frame.data = data + sizeof(esp_capture_stream_frame_t);
        out_frame.size = res->audio_frame_size;
        memcpy(data, &out_frame, sizeof(esp_capture_stream_frame_t));
        if (frame.size > 0) {
            ret = capture->enc_cfg.aenc->encode_frame(capture->enc_cfg.aenc, &frame, &out_frame);
        } else {
            out_frame.size = 0;
        }
        capture->src_cfg.release_src_frame(capture->src_cfg.src_ctx, &frame);
        if (ret != ESP_CAPTURE_ERR_OK) {
            ESP_LOGE(TAG, "Fail to encode audio frame");
            data_queue_send_buffer(res->audio_q, 0);
            continue;
        }
        data_queue_send_buffer(res->audio_q, out_frame.size + sizeof(esp_capture_stream_frame_t));
        capture->src_cfg.frame_processed(capture->src_cfg.src_ctx, ESP_CAPTURE_PATH_PRIMARY, &out_frame);
        if (frame.data == NULL && frame.size == 0) {
            ESP_LOGI(TAG, "Stop frame is received");
            break;
        }
    }
    ESP_LOGI(TAG, "Audio encoder thread exit");
    media_lib_event_group_set_bits(res->event_group, CAPTURE_AENC_EXITED);
    media_lib_thread_destroy(NULL);
}

static void simple_capture_venc_thread(void *arg)
{
    simple_capture_t *capture = (simple_capture_t *)arg;
    simple_capture_res_t *res = &capture->primary;
    esp_capture_stream_frame_t out_frame = {};
    out_frame.stream_type = ESP_CAPTURE_STREAM_TYPE_VIDEO;
    ESP_LOGI(TAG, "Enter video encoder thread");
    while (res->video_enabled) {
        // grab data from src
        esp_capture_stream_frame_t frame;
        frame.stream_type = ESP_CAPTURE_STREAM_TYPE_VIDEO;
        int ret = capture->src_cfg.acquire_src_frame(capture->src_cfg.src_ctx, &frame, false);
        if (ret != ESP_CAPTURE_ERR_OK) {
            ESP_LOGE(TAG, "Fail to acquire video frame ret %d", ret);
            break;
        }
        // TODO use original frame is OK?
        if (res->venc_bypass) {
            capture->src_cfg.frame_processed(capture->src_cfg.src_ctx, ESP_CAPTURE_PATH_PRIMARY, &frame);
            if (frame.data == NULL && frame.size == 0) {
                ESP_LOGI(TAG, "Stop frame is received");
                break;
            }
            continue;
        }
        // Get data for output
        int size = sizeof(esp_capture_stream_frame_t) + res->video_frame_size + VIDEO_ENC_OUT_ALIGNMENT;
        uint8_t *data = data_queue_get_buffer(res->video_q, size);
        if (data == NULL) {
            ESP_LOGE(TAG, "Fail to get video fifo buffer");
            capture->src_cfg.release_src_frame(capture->src_cfg.src_ctx, &frame);
            break;
        }
        out_frame.pts = frame.pts;
        out_frame.data = data + sizeof(esp_capture_stream_frame_t);
        // Align frame
        out_frame.data = (uint8_t *)ALIGN_UP((uintptr_t)out_frame.data, VIDEO_ENC_OUT_ALIGNMENT);
        out_frame.size = res->video_frame_size;
        memcpy(data, &out_frame, sizeof(esp_capture_stream_frame_t));
        if (frame.size) {
            ret = capture->enc_cfg.venc->encode_frame(capture->enc_cfg.venc, &frame, &out_frame);
        } else {
            out_frame.size = 0;
        }
        capture->src_cfg.release_src_frame(capture->src_cfg.src_ctx, &frame);
        if (ret != ESP_CAPTURE_ERR_OK) {
            if (ret == ESP_CAPTURE_ERR_NOT_ENOUGH) {
                ESP_LOGW(TAG, "Bad input maybe skipped");
                continue;
            }
            ESP_LOGE(TAG, "Fail to encode video frame");
            data_queue_send_buffer(res->video_q, 0);
            capture->src_cfg.event_cb(capture->src_cfg.src_ctx, ESP_CAPTURE_PATH_PRIMARY, ESP_CAPTURE_PATH_EVENT_VIDEO_ERROR);
            break;
        }
        size = (int)(intptr_t)(out_frame.data - (uint8_t *)data) + out_frame.size;
        data_queue_send_buffer(res->video_q, size);
        // Notify to use audio encoded frame
        capture->src_cfg.frame_processed(capture->src_cfg.src_ctx, ESP_CAPTURE_PATH_PRIMARY, &out_frame);
        if (frame.data == NULL && frame.size == 0) {
            ESP_LOGI(TAG, "Stop frame is received");
            break;
        }
    }
    ESP_LOGI(TAG, "Video encoder thread exit");
    media_lib_event_group_set_bits(res->event_group, CAPTURE_VENC_EXITED);
    media_lib_thread_destroy(NULL);
}

static int simple_capture_enable_audio(simple_capture_t *capture, bool enable)
{
    simple_capture_res_t *res = &capture->primary;
    if (res->sink.audio_info.codec == ESP_CAPTURE_CODEC_TYPE_NONE) {
        return ESP_CAPTURE_ERR_OK;
    }
    esp_capture_aenc_if_t *aenc = capture->enc_cfg.aenc;
    if (enable == false) {
        if (res->audio_enabled) {
            ESP_LOGI(TAG, "Start disable audio");
            res->audio_enabled = false;
            data_queue_consume_all(res->audio_q);
            media_lib_event_group_wait_bits(res->event_group, CAPTURE_AENC_EXITED, 100000);
            media_lib_event_group_clr_bits(res->event_group, CAPTURE_AENC_EXITED);
            aenc->stop(aenc);
            ESP_LOGI(TAG, "End to disable audio");
        }
        return ESP_CAPTURE_ERR_OK;
    }
    if (res->aenc_bypass == false) {
        int ret = aenc->start(aenc, &res->sink.audio_info);
        if (ret != ESP_CAPTURE_ERR_OK) {
            ESP_LOGE(TAG, "Fail to start audio encoder");
            return ret;
        }
        int in_frame_size = 0, out_frame_size = 0;
        aenc->get_frame_size(aenc, &in_frame_size, &out_frame_size);
        res->audio_frame_size = out_frame_size;
        int frame_count = capture->enc_cfg.aenc_frame_count ? capture->enc_cfg.aenc_frame_count : 5;
        int fifo_size = frame_count * (out_frame_size + 64);
        // Reuse queue if disable and enable again
        if (res->audio_q == NULL) {
            res->audio_q = data_queue_init(fifo_size);
        }
        if (res->audio_q == NULL) {
            ESP_LOGE(TAG, "Fail to init audio encoder fifo");
            return ESP_CAPTURE_ERR_NO_MEM;
        }
    }
    res->audio_enabled = true;
    media_lib_thread_handle_t thread = NULL;
    media_lib_thread_create_from_scheduler(&thread, "aenc", simple_capture_aenc_thread, capture);
    if (thread == NULL) {
        res->audio_enabled = false;
        return ESP_CAPTURE_ERR_NO_RESOURCES;
    }
    return ESP_CAPTURE_ERR_OK;
}

static int simple_capture_enable_video(simple_capture_t *capture, bool enable)
{
    simple_capture_res_t *res = &capture->primary;
    if (res->sink.video_info.codec == ESP_CAPTURE_CODEC_TYPE_NONE) {
        return ESP_CAPTURE_ERR_OK;
    }
    esp_capture_venc_if_t *venc = capture->enc_cfg.venc;
    if (enable == false) {
        if (res->video_enabled) {
            ESP_LOGI(TAG, "Start to disable video");
            res->video_enabled = false;
            data_queue_consume_all(res->video_q);
            media_lib_event_group_wait_bits(res->event_group, CAPTURE_VENC_EXITED, 10000);
            media_lib_event_group_clr_bits(res->event_group, CAPTURE_VENC_EXITED);
            venc->stop(venc);
            ESP_LOGI(TAG, "End to disable video");
        }
        return ESP_CAPTURE_ERR_OK;
    }
    int ret = ESP_CAPTURE_ERR_OK;
    if (res->venc_bypass == false) {
        ret = venc->start(venc, res->video_src_codec, &res->sink.video_info);
        if (ret != ESP_CAPTURE_ERR_OK) {
            ESP_LOGE(TAG, "Fail to start audio encoder");
            return ret;
        }
        int in_frame_size = 0, out_frame_size = 0;
        venc->get_frame_size(venc, &in_frame_size, &out_frame_size);
        res->video_frame_size = out_frame_size;
        int frame_count = capture->enc_cfg.venc_frame_count ? capture->enc_cfg.venc_frame_count : 2;
        int fifo_size = frame_count * (out_frame_size + 256);
        if (res->video_q == NULL) {
            res->video_q = data_queue_init(fifo_size);
        }
        if (res->video_q == NULL) {
            ESP_LOGE(TAG, "Fail to init video encoder fifo");
            return ESP_CAPTURE_ERR_NO_MEM;
        }
    }
    res->video_enabled = true;
    media_lib_thread_handle_t thread = NULL;
    media_lib_thread_create_from_scheduler(&thread, "venc", simple_capture_venc_thread, capture);
    if (thread == NULL) {
        res->video_enabled = false;
        ret = ESP_CAPTURE_ERR_NO_RESOURCES;
    }
    return ret;
}

int simple_capture_enable_path(esp_capture_path_if_t *p, esp_capture_path_type_t path, bool enable)
{
    simple_capture_t *capture = (simple_capture_t *)p;
    if (path != ESP_CAPTURE_PATH_PRIMARY) {
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    simple_capture_res_t *res = &capture->primary;
    if (res->enable == enable) {
        return ESP_CAPTURE_ERR_OK;
    }
    if (res->added == false) {
        ESP_LOGE(TAG, "Path not added yet");
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    res->enable = enable;
    int ret = ESP_CAPTURE_ERR_OK;
    if (res->started) {
        ret = simple_capture_enable_audio(capture, enable);
        if (ret != ESP_CAPTURE_ERR_OK) {
            capture->src_cfg.event_cb(capture->src_cfg.src_ctx, ESP_CAPTURE_PATH_PRIMARY, ESP_CAPTURE_PATH_EVENT_AUDIO_ERROR);
        }
        ret = simple_capture_enable_video(capture, enable);
        if (ret != ESP_CAPTURE_ERR_OK) {
            capture->src_cfg.event_cb(capture->src_cfg.src_ctx, ESP_CAPTURE_PATH_PRIMARY, ESP_CAPTURE_PATH_EVENT_VIDEO_ERROR);
        }
    }
    return ret;
}

int simple_capture_start(esp_capture_path_if_t *p)
{
    simple_capture_t *capture = (simple_capture_t *)p;
    simple_capture_res_t *res = &capture->primary;
    if (res->started) {
        return ESP_CAPTURE_ERR_OK;
    }
    if (res->sink.video_info.codec && check_video_codec_support(capture, capture->enc_cfg.venc, &res->sink.video_info) == false) {
        res->sink.video_info.codec = ESP_CAPTURE_CODEC_TYPE_NONE;
    }
    int ret = ESP_CAPTURE_ERR_OK;
    res->started = true;
    if (res->enable) {
        ret = simple_capture_enable_audio(capture, true);
        if (ret != ESP_CAPTURE_ERR_OK) {
            capture->src_cfg.event_cb(capture->src_cfg.src_ctx, ESP_CAPTURE_PATH_PRIMARY, ESP_CAPTURE_PATH_EVENT_AUDIO_ERROR);
        }
        ret = simple_capture_enable_video(capture, true);
        if (ret != ESP_CAPTURE_ERR_OK) {
            capture->src_cfg.event_cb(capture->src_cfg.src_ctx, ESP_CAPTURE_PATH_PRIMARY, ESP_CAPTURE_PATH_EVENT_VIDEO_ERROR);
        }
    }
    return ret;
}

int simple_capture_set(esp_capture_path_if_t *p, esp_capture_path_type_t path, esp_capture_path_set_type_t type, void *cfg, int cfg_size)
{
    simple_capture_t *capture = (simple_capture_t *)p;
    simple_capture_res_t *res = &capture->primary;
    int ret = ESP_CAPTURE_ERR_OK;
    switch (type) {
        case ESP_CAPTURE_PATH_SET_TYPE_AUDIO_BITRATE:
            if (res->aenc_bypass == false && capture->enc_cfg.aenc != NULL && cfg_size == sizeof(int)) {
                ret = capture->enc_cfg.aenc->set_bitrate(capture->enc_cfg.aenc, *(int *)cfg);
            }
            break;
        case ESP_CAPTURE_PATH_SET_TYPE_VIDEO_BITRATE:
            if (res->venc_bypass == false && capture->enc_cfg.venc != NULL && cfg_size == sizeof(int)) {
                ret = capture->enc_cfg.venc->set_bitrate(capture->enc_cfg.venc, *(int *)cfg);
            }
            break;
        case ESP_CAPTURE_PATH_SET_TYPE_VIDEO_FPS:
            break;
        default:
            return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    return ret;
}

int simple_capture_return_frame(esp_capture_path_if_t *p, esp_capture_path_type_t path, esp_capture_stream_frame_t *frame)
{
    simple_capture_t *capture = (simple_capture_t *)p;
    simple_capture_res_t *res = &capture->primary;
    int ret = ESP_CAPTURE_ERR_NOT_SUPPORTED;
    if (frame->stream_type == ESP_CAPTURE_STREAM_TYPE_AUDIO) {
        if (res->audio_enabled) {
            if (res->aenc_bypass) {
                capture->src_cfg.release_src_frame(capture->src_cfg.src_ctx, frame);
            } else {
                esp_capture_stream_frame_t *read_frame = NULL;
                if (data_queue_have_data(res->audio_q)) {
                    int read_size = 0;
                    data_queue_read_lock(res->audio_q, (void **)&read_frame, &read_size);
                    ESP_LOGD(TAG, "simple return audio data:%x frame:%x\n", frame->data[0], read_frame->data[0]);
                    ret = data_queue_read_unlock(res->audio_q);
                }
            }
        }
    } else if (frame->stream_type == ESP_CAPTURE_STREAM_TYPE_VIDEO) {
        if (res->video_enabled) {
            if (res->venc_bypass) {
                capture->src_cfg.release_src_frame(capture->src_cfg.src_ctx, frame);
            } else {
                esp_capture_stream_frame_t *read_frame = NULL;
                if (data_queue_have_data(res->video_q)) {
                    int read_size = 0;
                    data_queue_read_lock(res->video_q, (void **)&read_frame, &read_size);
                    ESP_LOGD(TAG, "simple return video data:%x frame:%x\n", frame->data[0], read_frame->data[0]);
                    ret = data_queue_read_unlock(res->video_q);
                }
            }
        }
    }
    return ret;
}

int simple_capture_stop(esp_capture_path_if_t *h)
{
    simple_capture_t *capture = (simple_capture_t *)h;
    int ret = ESP_CAPTURE_ERR_OK;
    simple_capture_res_t *res = &capture->primary;
    if (res->started == false) {
        return ESP_CAPTURE_ERR_OK;
    }
    simple_capture_enable_audio(capture, false);
    simple_capture_enable_video(capture, false);
    if (res->audio_q) {
        data_queue_deinit(res->audio_q);
        res->audio_q = NULL;
    }
    if (res->video_q) {
        data_queue_deinit(res->video_q);
        res->video_q = NULL;
    }
    res->started = false;
    return ret;
}

int simple_capture_close(esp_capture_path_if_t *h)
{
    simple_capture_t *capture = (simple_capture_t *)h;
    simple_capture_res_t *res = &capture->primary;
    simple_capture_stop(h);
    if (res->event_group) {
        media_lib_event_group_destroy(res->event_group);
        res->event_group = NULL;
    }
    return ESP_CAPTURE_ERR_OK;
}

esp_capture_path_if_t *esp_capture_build_simple_path(esp_capture_simple_path_cfg_t *cfg)
{
    simple_capture_t *capture = calloc(1, sizeof(simple_capture_t));
    if (capture == NULL) {
        return NULL;
    }
    capture->base.open = simple_capture_open;
    capture->base.add_path = simple_capture_add_path;
    capture->base.add_overlay = simple_capture_add_overlay;
    capture->base.enable_overlay = simple_capture_enable_overlay;
    capture->base.enable_path = simple_capture_enable_path;
    capture->base.get_audio_frame_samples = simple_capture_get_frame_samples;
    capture->base.start = simple_capture_start;
    capture->base.set = simple_capture_set;
    capture->base.return_frame = simple_capture_return_frame;
    capture->base.stop = simple_capture_stop;
    capture->base.close = simple_capture_close;
    capture->enc_cfg = *cfg;
    return &capture->base;
}
