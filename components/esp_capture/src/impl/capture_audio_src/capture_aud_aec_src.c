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
#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32P4
#include "esp_capture_types.h"
#include "esp_capture_audio_src_if.h"
#include "esp_capture_defaults.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "esp_aec.h"
#include "data_queue.h"
#include <string.h>
#include "media_lib_os.h"
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"

#define TAG "AUD_AEC_SRC"

// #define DISABLE_AEC

typedef struct {
    esp_capture_audio_src_if_t base;
    uint8_t                    channel;
    uint8_t                    channel_mask;
    esp_codec_dev_handle_t     handle;
    esp_capture_audio_info_t   info;
    uint64_t                   samples;
    bool                       start;
    bool                       open;

    data_queue_t             *in_q;
    uint8_t                  *cached_frame;
    int                       cached_read_pos;
    int                       cache_size;
    int                       cache_fill;
    bool                      in_quit;
    bool                      stopping;
    const esp_afe_sr_iface_t *aec_if;
    esp_afe_sr_data_t        *aec_data;
} audio_aec_src_t;

static int cal_frame_length(esp_capture_audio_info_t *info)
{
    // 16ms, 1channel, 16bit
    return 16 * info->sample_rate / 1000 * (16 / 8);
}

static int open_afe(audio_aec_src_t *src)
{
    afe_config_t afe_config = AFE_CONFIG_DEFAULT();
    afe_config.vad_init = false;
    afe_config.wakenet_init = false;
    afe_config.afe_perferred_core = 1;
    afe_config.afe_perferred_priority = 20;
    // afe_config.memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_INTERNAL;
    afe_config.pcm_config.mic_num = 1;
    afe_config.pcm_config.ref_num = 1;
    afe_config.pcm_config.total_ch_num = 1 + 1;
    afe_config.aec_init = true;
    afe_config.se_init = false;
#if 0
    afe_config.voice_communication_agc_init = true;
    afe_config.voice_communication_agc_gain = algo->agc_gain;
#endif
    afe_config.pcm_config.sample_rate = src->info.sample_rate;
    afe_config.voice_communication_init = true;
    src->aec_if = &ESP_AFE_VC_HANDLE;
    src->aec_data = src->aec_if->create_from_config(&afe_config);
    return 0;
}

static int audio_aec_src_open(esp_capture_audio_src_if_t *h)
{
    audio_aec_src_t *src = (audio_aec_src_t *)h;
    if (src->handle == NULL) {
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    src->samples = 0;
    src->open = true;
    return ESP_CAPTURE_ERR_OK;
}

static int audio_aec_src_get_support_codecs(esp_capture_audio_src_if_t *src, const esp_capture_codec_type_t **codecs, uint8_t *num)
{
    static esp_capture_codec_type_t support_codecs[] = { ESP_CAPTURE_CODEC_TYPE_PCM };
    *codecs = support_codecs;
    *num = 1;
    return ESP_CAPTURE_ERR_OK;
}

static int audio_aec_src_negotiate_caps(esp_capture_audio_src_if_t *h, esp_capture_audio_info_t *in_cap, esp_capture_audio_info_t *out_caps)
{
    audio_aec_src_t *src = (audio_aec_src_t *)h;
    // Only support 1 channel 16bits PCM
    if (in_cap->channel != 1 || in_cap->bits_per_sample != 16 || in_cap->codec != ESP_CAPTURE_CODEC_TYPE_PCM) {
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    *out_caps = *in_cap;
    src->info = *in_cap;
    return ESP_CAPTURE_ERR_OK;
}

static bool aec_dump_enabled = 0;
static uint8_t *origin_data = NULL;
static int origin_size;
static int origin_fill;
static uint8_t *aec_dump_data = NULL;
static int aec_dump_size = 0;
static int aec_dump_fill = 0;
static media_lib_mutex_handle_t dump_mutex;

static void enable_aec_dump(bool enable)
{
    if (origin_data == NULL) {
        origin_data = (uint8_t *)malloc(4096);
        if (origin_data == NULL) {
            ESP_LOGE(TAG, "Failed to malloc origin_data");
            return;
        }
        origin_size = 4096;
    }
    if (dump_mutex == NULL) {
        media_lib_mutex_create(&dump_mutex);
        if (dump_mutex == NULL) {
            return;
        }
    }
    if (aec_dump_data == NULL) {
        aec_dump_data = (uint8_t *)malloc(800 * 1024);
        if (aec_dump_data == NULL) {
            ESP_LOGE(TAG, "Failed to malloc AEC dump data");
            return;
        }
        aec_dump_size = 800 * 1024;
    }
    if (enable) {
        origin_fill = 0;
        aec_dump_fill = 0;
    }
    aec_dump_enabled = enable;
}

static void add_output_data(uint8_t *data, int size)
{
    if (aec_dump_enabled == false) {
        return;
    }
    int need_size = size * 3;
    if (aec_dump_fill + need_size > aec_dump_size || origin_fill < size * 2) {
        media_lib_mutex_lock(dump_mutex, 1000);
        origin_fill -= size * 2;
        media_lib_mutex_unlock(dump_mutex);
        return;
    }
    media_lib_mutex_lock(dump_mutex, 1000);
    int16_t *lr = (int16_t *)origin_data;
    int16_t *act = (int16_t *)data;
    int16_t *out = (int16_t *)(aec_dump_data + aec_dump_fill);
    int samples = size >> 1;
    while (samples > 0) {
        *(out++) = *(lr++);
        *(out++) = *(lr++);
        *(out++) = *(act++);
        samples--;
    }
    origin_fill -= size * 2;
    memmove(origin_data, origin_data + size * 2, origin_fill);
    aec_dump_fill += need_size;
    media_lib_mutex_unlock(dump_mutex);
}

static void add_origin_data(uint8_t *data, int size)
{
    if (aec_dump_enabled == false) {
        return;
    }
    if (origin_fill + size < origin_size) {
        media_lib_mutex_lock(dump_mutex, 1000);
        memcpy(origin_data + origin_fill, data, size);
        origin_fill += size;
        media_lib_mutex_unlock(dump_mutex);
    } else {
        ESP_LOGE(TAG, "Read too slow");
    }
}

#ifdef AEC_ADD_READ_THREAD
static void audio_read_thread(void *arg)
{
    audio_aec_src_t *src = (audio_aec_src_t *)arg;
    int read_size = src->cache_size * 2;
    while (1) {
        uint8_t *feed_data = (uint8_t *)data_queue_get_buffer(src->in_q, read_size);
        if (feed_data == NULL) {
            break;
        }
        int ret = esp_codec_dev_read(src->handle, feed_data, read_size);
        if (ret != 0) {
            ESP_LOGE(TAG, "Fail to read data %d", ret);
            data_queue_send_buffer(src->in_q, 0);
            break;
        }
        data_queue_send_buffer(src->in_q, read_size);
    }
    media_lib_thread_destroy(NULL);
}

static void audio_aec_src_buffer_in_thread(void *arg)
{
    audio_aec_src_t *src = (audio_aec_src_t *)arg;
    int read_size = src->cache_size * 2;
    src->in_q = data_queue_init(32 * 1024);
    int ret = -1;
    if (src->in_q) {
        ret = media_lib_thread_create_from_scheduler(NULL, "SrcRead", audio_read_thread, src);
    }
    int time_size = 0;
    while (!src->stopping && ret == 0) {
        void *feed_data = NULL;
        int ret = data_queue_read_lock(src->in_q, &feed_data, &read_size);
        if (feed_data == NULL) {
            break;
        }
        time_size += read_size;
        if (time_size > 32000) {
            time_size = 0;
            int q_num = 0, q_size = 0;
            data_queue_query(src->in_q, &q_num, &q_size);
            printf("Cached %d\n", q_size);
        }
        ret = src->aec_if->feed(src->aec_data, (int16_t *)feed_data);
        if (ret < 0) {
            ESP_LOGE(TAG, "Fail to feed data %d", ret);
            break;
        }
        add_origin_data(feed_data, read_size);
        data_queue_read_unlock(src->in_q);
    }
    printf("wait for read quit\n");
    if (src->in_q) {
        data_queue_wakeup(src->in_q);
        data_queue_deinit(src->in_q);
        src->in_q = NULL;
    }

    src->in_quit = true;
    printf("Quit done\n");
    media_lib_thread_destroy(NULL);
}

#else
static void audio_aec_src_buffer_in_thread(void *arg)
{
    audio_aec_src_t *src = (audio_aec_src_t *)arg;
    int read_size = src->cache_size * 2;
    uint8_t *feed_data = malloc(read_size);
    if (feed_data) {
        while (!src->stopping) {
            int ret = esp_codec_dev_read(src->handle, feed_data, read_size);
            if (ret != 0) {
                ESP_LOGE(TAG, "Fail to read data %d", ret);
                break;
            }
            ret = src->aec_if->feed(src->aec_data, (int16_t *)feed_data);
            if (ret < 0) {
                ESP_LOGE(TAG, "Fail to feed data %d", ret);
                break;
            }
            add_origin_data(feed_data, read_size);
        }
        free(feed_data);
    }
    src->in_quit = true;
    printf("Quit done\n");
    media_lib_thread_destroy(NULL);
}
#endif

static int audio_aec_src_start(esp_capture_audio_src_if_t *h)
{
    audio_aec_src_t *src = (audio_aec_src_t *)h;
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = src->info.sample_rate,
        .bits_per_sample = 16,
#ifdef DISABLE_AEC
        .channel = 1,
        .channel_mask = 1,
#else
        .channel = src->channel,
        .channel_mask = src->channel_mask,
#endif
    };
    src->in_quit = true;
    printf("Start to open channel %d mask %x\n", fs.channel, fs.channel_mask);
    int ret = esp_codec_dev_open(src->handle, &fs);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to open codec device, ret=%d", ret);
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
#ifndef DISABLE_AEC
    src->cache_size = cal_frame_length(&src->info);
    ret = open_afe(src);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to open AFE");
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    int audio_chunksize = src->aec_if->get_feed_chunksize(src->aec_data);
    printf("Audio chunksize %d\n", audio_chunksize);
    src->cache_size = audio_chunksize * (16 / 8);

    src->cached_frame = calloc(1, src->cache_size * 2);
    if (src->cached_frame == NULL) {
        ESP_LOGE(TAG, "Failed to allocate cache frame");
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    src->samples = 0;
    src->cached_read_pos = src->cache_fill = 0;
    src->stopping = false;

    media_lib_thread_handle_t thread = NULL;
    media_lib_thread_create_from_scheduler(&thread, "buffer_in", audio_aec_src_buffer_in_thread, src);
#endif
    src->start = true;
    src->in_quit = false;
    return ESP_CAPTURE_ERR_OK;
}

static int audio_aec_src_read_frame(esp_capture_audio_src_if_t *h, esp_capture_stream_frame_t *frame)
{
    audio_aec_src_t *src = (audio_aec_src_t *)h;
    if (src->start == false) {
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    frame->pts = (uint32_t)(src->samples * 1000 / src->info.sample_rate);
#ifndef DISABLE_AEC
    int need_size = frame->size;
    uint8_t *frame_data = frame->data;
    while (need_size > 0) {
        if (src->cached_read_pos < src->cache_fill) {
            int left = src->cache_fill - src->cached_read_pos;
            if (left > need_size) {
                left = need_size;
            }
            memcpy(frame_data, src->cached_frame + src->cached_read_pos, left);
            src->cached_read_pos += left;
            need_size -= left;
            frame_data += left;
            continue;
        }
        if (src->in_quit) {
            return ESP_CAPTURE_ERR_INTERNAL;
        }
        src->cache_fill = 0;
        src->cached_read_pos = 0;
        afe_fetch_result_t *res = src->aec_if->fetch(src->aec_data);
        if (res->ret_value != ESP_OK) {
            ESP_LOGE(TAG, "Fail to read from AEC");
            return -1;
        }
        if (res->data_size <= src->cache_size * 2) {
            memcpy(src->cached_frame, res->data, res->data_size);
            add_output_data(src->cached_frame, res->data_size);
            src->cache_fill = res->data_size;
        } else {
            ESP_LOGE(TAG, "Why so huge %d", res->data_size);
        }
    }
#else
    int ret = esp_codec_dev_read(src->handle, frame->data, frame->size);
    if (ret != 0) {
        ESP_LOGE(TAG, "Fail to read data %d", ret);
        return -1;
    }
    frame->pts = (uint32_t)(src->samples * 1000 / src->info.sample_rate);
#endif
    src->samples += frame->size / 2;
    return ESP_CAPTURE_ERR_OK;
}

static int audio_aec_src_stop(esp_capture_audio_src_if_t *h)
{
    audio_aec_src_t *src = (audio_aec_src_t *)h;
    printf("audio_aec_src_stop\n");
#ifndef DISABLE_AEC
    if (src->in_quit == false) {
        // fetch once
        src->aec_if->fetch(src->aec_data);
        src->stopping = true;
        while (src->in_quit == false) {
            media_lib_thread_sleep(10);
        }
    }
    if (src->aec_data) {
        src->aec_if->destroy(src->aec_data);
        src->aec_data = NULL;
    }
    if (src->cached_frame) {
        free(src->cached_frame);
        src->cached_frame = NULL;
    }
#endif
    if (src->handle) {
        esp_codec_dev_close(src->handle);
    }
    src->start = false;
    return ESP_CAPTURE_ERR_OK;
}

static int audio_aec_src_close(esp_capture_audio_src_if_t *h)
{
    audio_aec_src_t *src = (audio_aec_src_t *)h;
    src->handle = NULL;
    return ESP_CAPTURE_ERR_OK;
}

void esp_capture_enable_aec_src_dump(bool enable)
{
    enable_aec_dump(enable);
}

esp_capture_audio_src_if_t *esp_capture_new_audio_aec_src(esp_capture_audio_aec_src_cfg_t *cfg)
{
    if (cfg == NULL || cfg->record_handle == NULL) {
        return NULL;
    }
    audio_aec_src_t *src = calloc(1, sizeof(audio_aec_src_t));
    src->base.open = audio_aec_src_open;
    src->base.get_support_codecs = audio_aec_src_get_support_codecs;
    src->base.negotiate_caps = audio_aec_src_negotiate_caps;
    src->base.start = audio_aec_src_start;
    src->base.read_frame = audio_aec_src_read_frame;
    src->base.stop = audio_aec_src_stop;
    src->base.close = audio_aec_src_close;
    src->handle = cfg->record_handle;
    src->channel = cfg->channel ? cfg->channel : 2;
    src->channel_mask = cfg->channel_mask;
    return &src->base;
}

#endif
