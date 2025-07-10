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
#include "esp_capture.h"
#include "esp_log.h"
#include "esp_muxer.h"
#include "mp4_muxer.h"
#include "ts_muxer.h"
#include "msg_q.h"
#include "media_lib_os.h"
#include "data_queue.h"
#include "share_q.h"
#include "esp_capture_sync.h"
#include "msg_q.h"

#define TAG "ESP_CAPTURE"

#define SLICE_DURATION           300000
#define WRITE_CACHE_SIZE         (16 * 1024)
#define MUXER_DEFAULT_POOL_SIZE  (500 * 1024)
#define MIN_AUDIO_FRAME_DURATION 10
#define MIN_VIDEO_FRAME_DURATION 30

#define EVENT_GROUP_AUDIO_SRC_EXITED (1)
#define EVENT_GROUP_VIDEO_SRC_EXITED (2)
#define EVENT_GROUP_MUXER_EXITED     (4)

#define MAX_Q_SIZE (5)

// Here hacking to use stream type to indicate start/stop command
#define START_CMD_STREAM_TYPE  (esp_capture_stream_type_t)0x10
#define STOP_CMD_STREAM_TYPE   (esp_capture_stream_type_t)0x11
#define CAPTURE_SYNC_TOLERANCE 100
#define BREAK_SET_RETURN(ret_val) \
    ret = ret_val;                \
    break;

typedef enum {
    CAPTURE_SHARED_BY_USER  = 0,
    CAPTURE_SHARED_BY_MUXER = 1
} capture_shared_type_t;

struct capture_t;

typedef struct {
    esp_capture_path_type_t   path_type;
    esp_capture_sink_cfg_t    sink_cfg;
    esp_muxer_handle_t        muxer;
    esp_capture_muxer_cfg_t   muxer_cfg;
    data_queue_t             *muxer_data_q;
    bool                      muxer_enable;
    esp_capture_overlay_if_t *overlay;
    struct capture_t         *parent;
    bool                      run_once;
    bool                      run_finished;
    bool                      enable;
    bool                      audio_path_disabled;
    bool                      video_path_disabled;
    bool                      sink_disabled;
    bool                      muxing;
    bool                      muxer_started;
    msg_q_handle_t            audio_q;
    msg_q_handle_t            video_q;
    msg_q_handle_t            muxer_q;
    share_q_handle_t          audio_share_q;
    share_q_handle_t          video_share_q;
    uint32_t                  muxer_cur_pts;
    int                       audio_stream_idx;
    int                       video_stream_idx;
} capture_path_t;

typedef struct capture_t {
    esp_capture_cfg_t            cfg;
    uint32_t                     audio_frame_samples;
    uint32_t                     audio_frame_size;
    uint32_t                     audio_frames;
    uint32_t                     video_frames;
    esp_capture_audio_info_t     audio_src_info;
    esp_capture_video_info_t     video_src_info;
    data_queue_t                *audio_src_q;
    msg_q_handle_t               video_src_q;
    capture_path_t              *path[ESP_CAPTURE_PATH_MAX];
    uint8_t                      path_num;
    esp_capture_sync_handle_t    sync_handle;
    bool                         audio_nego_done;
    bool                         video_nego_done;
    bool                         fetching_audio;
    bool                         fetching_video;
    bool                         started;
    media_lib_event_grp_handle_t event_group;
    media_lib_mutex_handle_t     api_lock;
} capture_t;

static esp_muxer_type_t get_muxer_type(esp_capture_muxer_type_t muxer_type)
{
    switch (muxer_type) {
        case ESP_CAPTURE_MUXER_TYPE_MP4:
            return ESP_MUXER_TYPE_MP4;
        case ESP_CAPTURE_MUXER_TYPE_TS:
            return ESP_MUXER_TYPE_TS;
        default:
            return ESP_MUXER_TYPE_MAX;
    }
}

static esp_muxer_audio_codec_t get_muxer_acodec(esp_capture_codec_type_t codec_type)
{
    switch (codec_type) {
        case ESP_CAPTURE_CODEC_TYPE_AAC:
            return ESP_MUXER_ADEC_AAC;
        case ESP_CAPTURE_CODEC_TYPE_PCM:
            return ESP_MUXER_ADEC_PCM;
        default:
            return ESP_MUXER_ADEC_NONE;
    }
}

esp_muxer_video_codec_t get_muxer_vcodec(esp_capture_codec_type_t codec_type)
{
    switch (codec_type) {
        case ESP_CAPTURE_CODEC_TYPE_H264:
            return ESP_MUXER_VDEC_H264;
        case ESP_CAPTURE_CODEC_TYPE_MJPEG:
            return ESP_MUXER_VDEC_MJPEG;
        default:
            return ESP_MUXER_VDEC_NONE;
    }
}

static int muxer_data_reached(esp_muxer_data_info_t *muxer_data, void *ctx)
{
    capture_path_t *path = (capture_path_t *)ctx;
    if (path->muxer_cfg.capture_muxer_data && muxer_data->size) {
        // Add data to queue
        int size = sizeof(uint32_t) + muxer_data->size;
        void *data = data_queue_get_buffer(path->muxer_data_q, size);
        if (data) {
            *(uint32_t *)data = path->muxer_cur_pts;
            memcpy(data + sizeof(uint32_t), muxer_data->data, muxer_data->size);
            data_queue_send_buffer(path->muxer_data_q, size);
        }
    }
    return 0;
}

static bool muxer_support_streaming(esp_muxer_type_t muxer_type)
{
    if (muxer_type == ESP_MUXER_TYPE_TS || muxer_type == ESP_MUXER_TYPE_FLV) {
        return true;
    }
    return false;
}

static int open_muxer(capture_path_t *path)
{
    // TODO add other muxer types
    union {
        mp4_muxer_config_t mp4_cfg;
        ts_muxer_config_t ts_cfg;
    } muxer_cfg;
    memset(&muxer_cfg, 0, sizeof(muxer_cfg));
    esp_muxer_config_t *base = &muxer_cfg.mp4_cfg.base_config;
    base->muxer_type = get_muxer_type(path->muxer_cfg.muxer_type);
    base->slice_duration = SLICE_DURATION;
    base->url_pattern = path->muxer_cfg.slice_cb;
    if (path->muxer_cfg.capture_muxer_data) {
        if (muxer_support_streaming(base->muxer_type)) {
            base->data_cb = muxer_data_reached;
        } else {
            ESP_LOGE(TAG, "Muxer type %d does not support streaming", base->muxer_type);
            path->muxer_cfg.capture_muxer_data = false;
        }
    }
    base->ctx = path;
    // base->ram_cache_size = WRITE_CACHE_SIZE;
    int cfg_size = 0;
    if (base->muxer_type == ESP_MUXER_TYPE_MP4) {
        cfg_size = sizeof(muxer_cfg.mp4_cfg);
    } else if (base->muxer_type == ESP_MUXER_TYPE_TS) {
        cfg_size = sizeof(muxer_cfg.ts_cfg);
    }
    path->audio_stream_idx = -1;
    path->video_stream_idx = -1;
    path->muxer = esp_muxer_open(base, cfg_size);
    if (path->muxer == NULL) {
        ESP_LOGE(TAG, "Fail to open muxer");
        return ESP_CAPTURE_ERR_NO_MEM;
    }
    int ret = 0;
    esp_capture_sink_cfg_t *sink_cfg = &path->sink_cfg;
    if (sink_cfg->audio_info.codec && (path->muxer_cfg.muxer_mask == ESP_CAPTURE_MUXER_MASK_ALL || path->muxer_cfg.muxer_mask == ESP_CAPTURE_MUXER_MASK_AUDIO)) {
        esp_muxer_audio_stream_info_t audio_info = {
            .codec = get_muxer_acodec(sink_cfg->audio_info.codec),
            .sample_rate = sink_cfg->audio_info.sample_rate,
            .bits_per_sample = sink_cfg->audio_info.bits_per_sample,
            .channel = sink_cfg->audio_info.channel,
            .min_packet_duration = MIN_AUDIO_FRAME_DURATION,
        };
        ret = esp_muxer_add_audio_stream(path->muxer, &audio_info, &path->audio_stream_idx);
        if (ret != ESP_MUXER_ERR_OK) {
            ESP_LOGE(TAG, "Fail to add audio stream for muxer");
        }
    }
    if (sink_cfg->video_info.codec && (path->muxer_cfg.muxer_mask == ESP_CAPTURE_MUXER_MASK_ALL || path->muxer_cfg.muxer_mask == ESP_CAPTURE_MUXER_MASK_VIDEO)) {
        esp_muxer_video_stream_info_t video_info = {
            .codec = get_muxer_vcodec(sink_cfg->video_info.codec),
            .fps = sink_cfg->video_info.fps,
            .width = sink_cfg->video_info.width,
            .height = sink_cfg->video_info.height,
            .min_packet_duration = MIN_VIDEO_FRAME_DURATION,
        };
        ret = esp_muxer_add_video_stream(path->muxer, &video_info, &path->video_stream_idx);
        if (ret != ESP_MUXER_ERR_OK) {
            ESP_LOGE(TAG, "Fail to add audio stream for muxer");
        }
    }
    ret = (path->audio_stream_idx >= 0 || path->video_stream_idx >= 0) ? ESP_CAPTURE_ERR_OK : ESP_CAPTURE_ERR_NOT_SUPPORTED;
    if (ret != ESP_CAPTURE_ERR_OK) {
        esp_muxer_close(path->muxer);
        path->muxer = NULL;
        path->muxer_cfg.muxer_type = ESP_CAPTURE_MUXER_TYPE_NONE;
    }
    return ret;
}

static void muxer_thread(void *arg)
{
    capture_path_t *path = (capture_path_t *)arg;
    esp_capture_stream_frame_t frame = { 0 };
    ESP_LOGI(TAG, "Enter muxer thread muxing %d", path->muxing);
    while (path->muxing) {
        int ret = msg_q_recv(path->muxer_q, &frame, sizeof(frame), false);
        if (ret != 0) {
            ESP_LOGI(TAG, "Quit muxer for recv ret %d", ret);
            break;
        }
        if (frame.stream_type == STOP_CMD_STREAM_TYPE) {
            ESP_LOGI(TAG, "Muxer receive stop");
            break;
        }
        if (frame.data == NULL || frame.size == 0) {
            ESP_LOGE(TAG, "Receive quit frame");
            continue;
        }
        switch (frame.stream_type) {
            case ESP_CAPTURE_STREAM_TYPE_AUDIO: {
                esp_muxer_audio_packet_t audio_packet = {
                    .pts = frame.pts,
                    .data = frame.data,
                    .len = frame.size,
                };
                path->muxer_cur_pts = frame.pts;
                ret = esp_muxer_add_audio_packet(path->muxer, path->audio_stream_idx, &audio_packet);
                share_q_release(path->audio_share_q, &frame);
            } break;
            case ESP_CAPTURE_STREAM_TYPE_VIDEO: {
                esp_muxer_video_packet_t video_packet = {
                    .pts = frame.pts,
                    .data = frame.data,
                    .len = frame.size,
                };
                path->muxer_cur_pts = frame.pts;
                ret = esp_muxer_add_video_packet(path->muxer, path->video_stream_idx, &video_packet);
                share_q_release(path->video_share_q, &frame);
            } break;
            default:
                break;
        }
    }
    ESP_LOGI(TAG, "Leave muxer thread");
    media_lib_event_group_set_bits(path->parent->event_group, EVENT_GROUP_MUXER_EXITED);
    media_lib_thread_destroy(NULL);
}

static uint32_t calc_audio_pts(capture_t *capture, uint32_t frames)
{
    if (capture->audio_src_info.sample_rate == 0) {
        return 0;
    }
    return (uint32_t)((uint64_t)frames * capture->audio_frame_samples * 1000 / capture->audio_src_info.sample_rate);
}

static bool has_active_path(capture_t *capture, esp_capture_stream_type_t type, bool check_finished)
{
    if (type == ESP_CAPTURE_STREAM_TYPE_AUDIO) {
        for (int i = 0; i < capture->path_num; i++) {
            capture_path_t *path = capture->path[i];
            if (path->enable && path->sink_cfg.audio_info.codec && path->audio_path_disabled == false) {
                if (path->run_finished == false && check_finished) {
                    continue;
                }
                return true;
            }
        }
    } else if (type == ESP_CAPTURE_STREAM_TYPE_VIDEO) {
        for (int i = 0; i < capture->path_num; i++) {
            capture_path_t *path = capture->path[i];
            if (path->enable && path->sink_cfg.video_info.codec && path->video_path_disabled == false) {
                if (path->run_finished == false && check_finished) {
                    continue;
                }
                return true;
            }
        }
    }
    return false;
}

static void audio_src_thread(void *arg)
{
    capture_t *capture = (capture_t *)arg;
    ESP_LOGI(TAG, "Start to fetch audio src data now");
    while (capture->fetching_audio) {
        // No active path drop data directly
        if (has_active_path(capture, ESP_CAPTURE_STREAM_TYPE_AUDIO, false) == false) {
            media_lib_thread_sleep(10);
            continue;
        }
        // TODO how to calculate audio_frame_size
        int frame_size = sizeof(esp_capture_stream_frame_t) + capture->audio_frame_size;
        uint8_t *data = data_queue_get_buffer(capture->audio_src_q, frame_size);
        if (data == NULL) {
            ESP_LOGE(TAG, "Failed to get buffer from audio src queue");
            break;
        }
        esp_capture_stream_frame_t *frame = (esp_capture_stream_frame_t *)data;
        frame->stream_type = ESP_CAPTURE_STREAM_TYPE_AUDIO;
        frame->data = (data + sizeof(esp_capture_stream_frame_t));
        frame->size = capture->audio_frame_size;
        int ret = capture->cfg.audio_src->read_frame(capture->cfg.audio_src, frame);
        frame->pts = calc_audio_pts(capture, capture->audio_frames);
        // printf("Audio Frame %d to pts:%d size:%d\n", (int)capture->audio_frames, (int)frame->pts, (int)capture->audio_frame_size);
        if (ret != ESP_CAPTURE_ERR_OK) {
            data_queue_send_buffer(capture->audio_src_q, 0);
            ESP_LOGE(TAG, "Failed to read audio frame ret %d", ret);
            break;
        }
        if (capture->sync_handle) {
            esp_capture_sync_audio_update(capture->sync_handle, frame->pts);
            if (capture->cfg.sync_mode != ESP_CAPTURE_SYNC_MODE_AUDIO) {
                uint32_t cur_pts = 0;
                esp_capture_sync_get_current(capture->sync_handle, &cur_pts);
                if (frame->pts > cur_pts + CAPTURE_SYNC_TOLERANCE || frame->pts + CAPTURE_SYNC_TOLERANCE < cur_pts) {
                    frame->pts = cur_pts;
                }
            }
        }
        data_queue_send_buffer(capture->audio_src_q, frame_size);
        capture->audio_frames++;
    }
    ESP_LOGI(TAG, "Audio src thread exited");
    media_lib_event_group_set_bits(capture->event_group, EVENT_GROUP_AUDIO_SRC_EXITED);
    media_lib_thread_destroy(NULL);
}

static uint32_t calc_video_pts(capture_t *capture, uint32_t frames)
{
    if (capture->video_src_info.fps == 0) {
        return 0;
    }
    return (uint32_t)((uint64_t)frames * 1000 / capture->video_src_info.fps);
}

static bool is_encoded_video(esp_capture_codec_type_t codec)
{
    return (codec == ESP_CAPTURE_CODEC_TYPE_H264 || codec == ESP_CAPTURE_CODEC_TYPE_MJPEG);
}

static int capture_frame_processed(void *src, esp_capture_path_type_t sel, esp_capture_stream_frame_t *frame)
{
    capture_t *capture = (capture_t *)src;
    if (capture == NULL || sel >= ESP_CAPTURE_PATH_MAX || capture->path[sel] == NULL) {
        return ESP_CAPTURE_ERR_NOT_FOUND;
    }
    capture_path_t *path = capture->path[sel];
    // When path disable drop input data directly
    if (path->sink_disabled) {
        // can not release it here, let callback to handle it
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    int ret = ESP_CAPTURE_ERR_NOT_SUPPORTED;
    switch (frame->stream_type) {
        default:
            break;
        case ESP_CAPTURE_STREAM_TYPE_VIDEO:
            if (path->video_share_q) {
                ret = share_q_add(path->video_share_q, frame);
            }
            break;
        case ESP_CAPTURE_STREAM_TYPE_AUDIO:
            if (path->audio_share_q) {
                ret = share_q_add(path->audio_share_q, frame);
            }
            break;
    }
    return ret;
}

static int capture_path_event_reached(void *src, esp_capture_path_type_t sel, esp_capture_path_event_type_t event)
{
    capture_t *capture = (capture_t *)src;
    if (capture == NULL || sel >= ESP_CAPTURE_PATH_MAX || capture->path[sel] == NULL) {
        return ESP_CAPTURE_ERR_NOT_FOUND;
    }
    capture_path_t *path = capture->path[sel];
    switch (event) {
        default:
            break;
        case ESP_CAPTURE_PATH_EVENT_AUDIO_NOT_SUPPORT:
        case ESP_CAPTURE_PATH_EVENT_AUDIO_ERROR: {
            path->audio_path_disabled = true;
            // TODO send fake data into share queue can let user quit
            // But will cause wrong share queue release not existed data
            if (path->audio_share_q) {
                esp_capture_stream_frame_t frame = { 0 };
                frame.stream_type = ESP_CAPTURE_STREAM_TYPE_AUDIO;
                share_q_add(path->audio_share_q, &frame);
            }
            break;
        }
        case ESP_CAPTURE_PATH_EVENT_VIDEO_NOT_SUPPORT:
        case ESP_CAPTURE_PATH_EVENT_VIDEO_ERROR: {
            path->video_path_disabled = true;
            if (path->video_share_q) {
                esp_capture_stream_frame_t frame = { 0 };
                frame.stream_type = ESP_CAPTURE_STREAM_TYPE_VIDEO;
                share_q_add(path->video_share_q, &frame);
            }
            break;
        }
    }
    return ESP_CAPTURE_ERR_OK;
}

static void video_src_thread(void *arg)
{
    capture_t *capture = (capture_t *)arg;
    esp_capture_stream_frame_t frame = {
        .stream_type = ESP_CAPTURE_STREAM_TYPE_VIDEO,
    };
    ESP_LOGI(TAG, "Start to fetch video src data now");
    while (capture->fetching_video) {
        if (has_active_path(capture, ESP_CAPTURE_STREAM_TYPE_VIDEO, false) == false) {
            media_lib_thread_sleep(10);
            continue;
        }
        int ret = capture->cfg.video_src->acquire_frame(capture->cfg.video_src, &frame);
        if (ret != ESP_CAPTURE_ERR_OK) {
            ESP_LOGE(TAG, "Failed to acquire video frame");
            break;
        }
        uint32_t video_pts = calc_video_pts(capture, capture->video_frames);
        // TODO not drop if is raw
        if (capture->sync_handle) {
            uint32_t cur_pts = 0;
            esp_capture_sync_get_current(capture->sync_handle, &cur_pts);
            if (video_pts > cur_pts && is_encoded_video(capture->video_src_info.codec) == false) {
                // Drop current video
                capture->cfg.video_src->release_frame(capture->cfg.video_src, &frame);
                continue;
            } else if (video_pts + CAPTURE_SYNC_TOLERANCE < cur_pts) {
                // Video too slow force to use current pts
                video_pts = cur_pts;
            }
        }
        capture->video_frames++;
        frame.pts = video_pts;
        ret = msg_q_send(capture->video_src_q, &frame, sizeof(esp_capture_stream_frame_t));
        if (ret != 0) {
            ESP_LOGE(TAG, "Failed to send video frame to queue");
            capture->cfg.video_src->release_frame(capture->cfg.video_src, &frame);
            break;
        }
    }
    ESP_LOGI(TAG, "Video src thread exited");
    media_lib_event_group_set_bits(capture->event_group, EVENT_GROUP_VIDEO_SRC_EXITED);
    media_lib_thread_destroy(NULL);
}

static int capture_path_acquire_frame(void *src, esp_capture_stream_frame_t *frame, bool no_wait)
{
    capture_t *capture = (capture_t *)src;
    int ret = ESP_CAPTURE_ERR_NOT_SUPPORTED;
    switch (frame->stream_type) {
        default:
            break;
        case ESP_CAPTURE_STREAM_TYPE_AUDIO:
            if (capture->audio_src_q) {
                void *data;
                int size;
                if (no_wait && data_queue_have_data(capture->audio_src_q) == false) {
                    ret = ESP_CAPTURE_ERR_NOT_FOUND;
                    break;
                }
                ret = data_queue_read_lock(capture->audio_src_q, &data, &size);
                if (ret != 0 || data == NULL) {
                    ret = ESP_CAPTURE_ERR_INTERNAL;
                    break;
                }
                memcpy(frame, data, sizeof(esp_capture_stream_frame_t));
                ret = ESP_CAPTURE_ERR_OK;
            }
            break;
        case ESP_CAPTURE_STREAM_TYPE_VIDEO:
            if (capture->video_src_q) {
                ret = msg_q_recv(capture->video_src_q, frame, sizeof(esp_capture_stream_frame_t), no_wait);
                if (ret != 0) {
                    ret = ESP_CAPTURE_ERR_INTERNAL;
                }
            }
            break;
    }
    return ret;
}

static int capture_path_release_frame(void *src, esp_capture_stream_frame_t *frame)
{
    capture_t *capture = (capture_t *)src;
    int ret = ESP_CAPTURE_ERR_NOT_SUPPORTED;
    switch (frame->stream_type) {
        default:
            break;
        case ESP_CAPTURE_STREAM_TYPE_AUDIO:
            if (capture->audio_src_q) {
                data_queue_read_unlock(capture->audio_src_q);
                ret = ESP_CAPTURE_ERR_OK;
            }
            break;
        case ESP_CAPTURE_STREAM_TYPE_VIDEO:
            if (capture->video_src_q && frame->data) {
                // Release video frame here
                capture->cfg.video_src->release_frame(capture->cfg.video_src, frame);
                ret = ESP_CAPTURE_ERR_OK;
            }
            break;
    }
    return ret;
}

static int capture_path_nego_video_caps(void *src, esp_capture_video_info_t *in_cap, esp_capture_video_info_t *out_caps)
{
    capture_t *capture = (capture_t *)src;
    if (capture->cfg.video_src == NULL) {
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    // TODO save out_caps to capture?
    int ret = capture->cfg.video_src->negotiate_caps(capture->cfg.video_src, in_cap, out_caps);
    if (ret == ESP_CAPTURE_ERR_OK) {
        memcpy(&capture->video_src_info, out_caps, sizeof(esp_capture_video_info_t));
        capture->video_nego_done = true;
        ESP_LOGI(TAG, "Video source caps negotiate done: %dx%d@%dfps", (int)out_caps->width, (int)out_caps->height, (int)out_caps->fps);
    }
    return ret;
}

static int capture_path_nego_audio_caps(void *src, esp_capture_audio_info_t *in_cap, esp_capture_audio_info_t *out_caps)
{
    capture_t *capture = (capture_t *)src;
    if (capture->cfg.audio_src == NULL) {
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    // TODO save out_caps to capture?
    int ret = capture->cfg.audio_src->negotiate_caps(capture->cfg.audio_src, in_cap, out_caps);
    if (ret == ESP_CAPTURE_ERR_OK) {
        memcpy(&capture->audio_src_info, out_caps, sizeof(esp_capture_audio_info_t));
        ESP_LOGI(TAG, "Audio source caps negotiate done: %dHz, %d channels", (int)out_caps->sample_rate, (int)out_caps->channel);
        capture->audio_nego_done = true;
    }
    return ret;
}

static void *video_sink_get_q_data_ptr(void *item)
{
    esp_capture_stream_frame_t *frame = (esp_capture_stream_frame_t *)item;
    return frame ? frame->data : NULL;
}

static void *audio_sink_get_q_data_ptr(void *item)
{
    esp_capture_stream_frame_t *frame = (esp_capture_stream_frame_t *)item;
    return frame ? frame->data : NULL;
}

static int video_sink_release_frame(void *item, void *ctx)
{
    esp_capture_stream_frame_t *frame = (esp_capture_stream_frame_t *)item;
    capture_path_t *path = (capture_path_t *)ctx;
    esp_capture_path_if_t *capture_path = path->parent->cfg.capture_path;
    int ret = capture_path->return_frame(capture_path, path->path_type, frame);
    if (path->run_once) {
        path->run_finished = true;
        ESP_LOGI(TAG, "Capture once finished");
    }
    return ret;
}

static int audio_sink_release_frame(void *item, void *ctx)
{
    esp_capture_stream_frame_t *frame = (esp_capture_stream_frame_t *)item;
    capture_path_t *path = (capture_path_t *)ctx;
    esp_capture_path_if_t *capture_path = path->parent->cfg.capture_path;
    ESP_LOGD(TAG, "Begin to return audio frame");
    return capture_path->return_frame(capture_path, path->path_type, frame);
}

static int stop_muxer(capture_path_t *path)
{
    // Disable data sending firstly
    if (path->video_share_q) {
        share_q_enable(path->video_share_q, CAPTURE_SHARED_BY_MUXER, false);
    }
    if (path->audio_share_q) {
        share_q_enable(path->audio_share_q, CAPTURE_SHARED_BY_MUXER, false);
    }
    // Wait for thread to quit
    if (path->muxing) {
        esp_capture_stream_frame_t frame = { 0 };
        frame.stream_type = STOP_CMD_STREAM_TYPE;
        msg_q_send(path->muxer_q, &frame, sizeof(frame));
        media_lib_event_group_wait_bits(path->parent->event_group, EVENT_GROUP_MUXER_EXITED, 1000);
        path->muxing = false;
    }
    // Close muxer
    if (path->muxer) {
        esp_muxer_close(path->muxer);
        path->muxer = NULL;
    }
    path->muxer_started = false;
    return ESP_CAPTURE_ERR_OK;
}

static int start_muxer(capture_path_t *path)
{
    if (path->enable == false || path->parent->started == false) {
        return ESP_CAPTURE_ERR_OK;
    }
    if (path->muxer_started) {
        return ESP_CAPTURE_ERR_OK;
    }
    int ret = open_muxer(path);
    do {
        if (ret != ESP_CAPTURE_ERR_OK) {
            ESP_LOGE(TAG, "Fail to open muxer");
            break;
        }
        path->muxing = true;
        media_lib_thread_handle_t handle;
        ret = media_lib_thread_create_from_scheduler(&handle, "Muxer", muxer_thread, path);
        if (ret != 0) {
            ESP_LOGE(TAG, "Fail to create muxer thread");
            path->muxer_enable = false;
            path->muxing = false;
            ret = ESP_CAPTURE_ERR_NO_RESOURCES;
            break;
        }
    } while (0);
    // Enable muxer when muxer is ready
    if (path->video_share_q) {
        bool enable = path->muxer_enable && (path->video_stream_idx >= 0);
        share_q_enable(path->video_share_q, CAPTURE_SHARED_BY_MUXER, enable);
    }
    if (path->audio_share_q) {
        bool enable = path->muxer_enable && (path->audio_stream_idx >= 0);
        share_q_enable(path->audio_share_q, CAPTURE_SHARED_BY_MUXER, enable);
    }
    if (ret != ESP_CAPTURE_ERR_OK) {
        return ret;
    }
    path->muxer_started = true;
    return ret;
}

static int enable_muxer(capture_path_t *path, bool enable)
{
    if (path->muxer_enable == enable) {
        return ESP_CAPTURE_ERR_OK;
    }
    path->muxer_enable = enable;
    int ret;
    if (enable) {
        ret = start_muxer(path);
    } else {
        ret = stop_muxer(path);
    }
    return ret;
}

static int prepare_src_queue(capture_path_t *path)
{
    capture_t *capture = path->parent;
    if (path->sink_cfg.audio_info.codec != ESP_CAPTURE_CODEC_TYPE_NONE && capture->audio_src_q == NULL) {
        // TODO need configure audio src q
        capture->audio_src_q = data_queue_init(10 * 1024);
        if (capture->audio_src_q == NULL) {
            ESP_LOGE(TAG, "Failed to create audio src q");
            // Not support audio now
            path->sink_cfg.audio_info.codec = ESP_CAPTURE_CODEC_TYPE_NONE;
        }
    }

    if (path->sink_cfg.video_info.codec != ESP_CAPTURE_CODEC_TYPE_NONE && capture->video_src_q == NULL) {
        // TODO need refine queue number
        capture->video_src_q = msg_q_create(5, sizeof(esp_capture_stream_frame_t));
        if (capture->video_src_q == NULL) {
            ESP_LOGE(TAG, "Failed to create video src q");
            path->sink_cfg.video_info.codec = ESP_CAPTURE_CODEC_TYPE_NONE;
        }
    }
    return (path->sink_cfg.video_info.codec || path->sink_cfg.audio_info.codec) ? ESP_CAPTURE_ERR_OK : ESP_CAPTURE_ERR_NO_RESOURCES;
}

static int start_path(capture_path_t *path)
{
    capture_t *capture = path->parent;
    // Do not prepare resource when not started yet
    if (capture->started == false) {
        return ESP_CAPTURE_ERR_OK;
    }
    // Prepare source queues firstly
    int ret = prepare_src_queue(path);
    if (ret != ESP_CAPTURE_ERR_OK) {
        ESP_LOGE(TAG, "Failed to prepare src queue");
        return ret;
    }
    if (capture->cfg.capture_path == NULL) {
        return ESP_CAPTURE_ERR_OK;
    }
    // Clear path error status
    path->audio_path_disabled = false;
    path->video_path_disabled = false;

    if (path->muxer_cfg.muxer_type && path->muxer_q == NULL) {
        path->muxer_q = msg_q_create(10, sizeof(esp_capture_stream_frame_t));
        if (path->muxer_q == NULL) {
            ESP_LOGE(TAG, "Failed to create muxer q");
            path->muxer_cfg.muxer_type = ESP_CAPTURE_MUXER_TYPE_NONE;
        }
    }
    // Create audio share queues and audio sink queue to hold sink output data
    if (path->sink_cfg.audio_info.codec) {
        if (path->muxer_cfg.muxer_only == false && path->audio_q == NULL) {
            path->audio_q = msg_q_create(5, sizeof(esp_capture_stream_frame_t));
            if (path->audio_q == NULL) {
                ESP_LOGE(TAG, "Failed to create audio q");
            }
        }
        // TODO support muxer when path_if not existed
        if (capture->audio_src_q && capture->cfg.capture_path) {
            // Create share queue to hold sink buffer
            if (path->audio_share_q == NULL) {
                uint8_t user_count = 1;
                // Support muxer add user
                if (path->muxer_cfg.muxer_type) {
                    user_count++;
                }
                share_q_cfg_t cfg = {
                    .user_count = user_count,
                    .q_count = 5,
                    .item_size = sizeof(esp_capture_stream_frame_t),
                    .get_frame_data = audio_sink_get_q_data_ptr,
                    .release_frame = audio_sink_release_frame,
                    .ctx = path,
                    .use_external_q = true,
                };
                path->audio_share_q = share_q_create(&cfg);
            }
            if (path->audio_share_q == NULL) {
                ESP_LOGE(TAG, "Failed to create share q for audio sink");
            } else {
                if (path->audio_q) {
                    share_q_set_external(path->audio_share_q, CAPTURE_SHARED_BY_USER, path->audio_q);
                    share_q_enable(path->audio_share_q, CAPTURE_SHARED_BY_USER, path->enable);
                }
                if (path->muxer_q) {
                    share_q_set_external(path->audio_share_q, CAPTURE_SHARED_BY_MUXER, path->muxer_q);
                    share_q_enable(path->audio_share_q, CAPTURE_SHARED_BY_MUXER, path->muxer_enable);
                }
            }
        }
    }
    // Create video share queues and video sink queue to hold sink output data
    if (path->sink_cfg.video_info.codec) {
        if (path->muxer_cfg.muxer_only == false && path->video_q == NULL) {
            path->video_q = msg_q_create(5, sizeof(esp_capture_stream_frame_t));
        }
        if (capture->video_src_q && capture->cfg.capture_path) {
            // Create share queue to hold sink buffer
            if (path->video_share_q == NULL) {
                uint8_t user_count = 1;
                if (path->muxer_cfg.muxer_type) {
                    user_count++;
                }
                share_q_cfg_t cfg = {
                    .user_count = user_count,
                    .q_count = 5,
                    .item_size = sizeof(esp_capture_stream_frame_t),
                    .get_frame_data = video_sink_get_q_data_ptr,
                    .release_frame = video_sink_release_frame,
                    .ctx = path,
                    .use_external_q = true,
                };
                path->video_share_q = share_q_create(&cfg);
            }
            if (path->video_share_q == NULL) {
                ESP_LOGE(TAG, "Failed to create share q for video sink");
            } else {
                if (path->video_q) {
                    share_q_set_external(path->video_share_q, CAPTURE_SHARED_BY_USER, path->video_q);
                    share_q_enable(path->video_share_q, CAPTURE_SHARED_BY_USER, path->enable);
                }
                if (path->muxer_q) {
                    share_q_set_external(path->video_share_q, CAPTURE_SHARED_BY_MUXER, path->muxer_q);
                    share_q_enable(path->video_share_q, CAPTURE_SHARED_BY_MUXER, path->muxer_enable);
                }
            }
        }
    }
    if (path->muxer_q) {
        // Create muxer output queue if user want to fetch muxer data also
        if (path->muxer_cfg.capture_muxer_data) {
            int muxer_pool_size = MUXER_DEFAULT_POOL_SIZE;
            if (path->muxer_cfg.muxer_cache_size) {
                muxer_pool_size = path->muxer_cfg.muxer_cache_size;
            }
            if (path->muxer_data_q == NULL) {
                path->muxer_data_q = data_queue_init(muxer_pool_size);
                if (path->muxer_data_q == NULL) {
                    ESP_LOGE(TAG, "Fail to create output queue for muxer");
                }
            }
        }
        if (path->muxer_data_q) {
            start_muxer(path);
        }
    }
    return (path->audio_share_q || path->video_share_q) ? ESP_CAPTURE_ERR_OK : ESP_CAPTURE_ERR_NO_MEM;
}

static void release_path(capture_path_t *path)
{
    if (path->audio_q) {
        msg_q_destroy(path->audio_q);
        path->audio_q = NULL;
    }
    if (path->video_q) {
        msg_q_destroy(path->video_q);
        path->video_q = NULL;
    }
    if (path->muxer_q) {
        msg_q_destroy(path->muxer_q);
        path->muxer_q = NULL;
    }
    // Start to destroy queue
    if (path->muxer_data_q) {
        data_queue_deinit(path->muxer_data_q);
        path->muxer_data_q = NULL;
    }
    if (path->audio_share_q) {
        share_q_destroy(path->audio_share_q);
        path->audio_share_q = NULL;
    }
    if (path->video_share_q) {
        share_q_destroy(path->video_share_q);
        path->video_share_q = NULL;
    }
}

static void consume_all_audio_src(capture_t *capture)
{
    if (capture->audio_src_q == NULL) {
        return;
    }
    data_queue_consume_all(capture->audio_src_q);
}

static void consume_all_video_src(capture_t *capture)
{
    if (capture->video_src_q == NULL) {
        return;
    }
    esp_capture_stream_frame_t frame = {};
    while (msg_q_recv(capture->video_src_q, &frame, sizeof(frame), true) == 0) {
        if (frame.size) {
            capture->cfg.video_src->release_frame(capture->cfg.video_src, &frame);
        }
    }
}

static int stop_path(capture_path_t *path)
{
    if (path->video_share_q) {
        share_q_enable(path->video_share_q, CAPTURE_SHARED_BY_USER, false);
    }
    if (path->audio_share_q) {
        share_q_enable(path->audio_share_q, CAPTURE_SHARED_BY_USER, false);
    }
    // Receive all data in src queue
    if (!has_active_path(path->parent, ESP_CAPTURE_STREAM_TYPE_AUDIO, false)) {
        consume_all_audio_src(path->parent);
    }
    if (!has_active_path(path->parent, ESP_CAPTURE_STREAM_TYPE_VIDEO, false)) {
        consume_all_video_src(path->parent);
    }
    return ESP_CAPTURE_ERR_OK;
}

static int capture_negotiate_directly(capture_t *capture, esp_capture_sink_cfg_t *sink_info)
{
    // Query src support it
    int ret = ESP_CAPTURE_ERR_OK;
    if (sink_info->audio_info.codec) {
        ret = capture_path_nego_audio_caps(capture, &sink_info->audio_info, &capture->audio_src_info);
        if (ret != ESP_CAPTURE_ERR_OK) {
            ESP_LOGE(TAG, "Audio src not support codec:%d sample_rate:%d channel:%d",
                     (int)sink_info->audio_info.codec, (int)sink_info->audio_info.sample_rate, (int)sink_info->audio_info.channel);
            return ret;
        }
    }
    if (sink_info->video_info.codec) {
        ret = capture_path_nego_video_caps(capture, &sink_info->video_info, &capture->video_src_info);
        if (ret != ESP_CAPTURE_ERR_OK) {
            ESP_LOGE(TAG, "Video src not support codec:%d resolution:%dx%d",
                     (int)sink_info->video_info.codec, (int)sink_info->video_info.width, (int)sink_info->video_info.height);
            return ret;
        }
    }
    return ret;
}

static int capture_update_audio_frame_samples(capture_t *capture)
{
    esp_capture_audio_info_t *aud_info = &capture->audio_src_info;
    int samples = 20 * aud_info->sample_rate / 1000;
    int sample_size = aud_info->bits_per_sample * aud_info->channel >> 3;
    capture->audio_frame_samples = samples;
    capture->audio_frame_size = samples * sample_size;

    if (capture->cfg.capture_path == NULL) {
        return ESP_CAPTURE_ERR_OK;
    }
    int path_samples = 0;
    // Use min frame size for low latency output
    for (int i = 0; i < capture->path_num; i++) {
        capture_path_t *path = capture->path[i];
        if (path->enable == false || path->sink_cfg.audio_info.codec == ESP_CAPTURE_CODEC_TYPE_NONE) {
            continue;
        }
        // Prepare queues and related resource
        int need_sample = capture->cfg.capture_path->get_audio_frame_samples(capture->cfg.capture_path, path->path_type);
        if (need_sample > 0) {
            if (path_samples == 0) {
                path_samples = need_sample;
            } else if (need_sample < path_samples) {
                path_samples = need_sample;
            }
        }
    }
    if (path_samples) {
        capture->audio_frame_samples = path_samples;
        capture->audio_frame_size = path_samples * sample_size;
    }
    return ESP_CAPTURE_ERR_OK;
}

static int capture_start_src(capture_t *capture)
{
    int ret = ESP_CAPTURE_ERR_OK;
    media_lib_thread_handle_t handle = NULL;
    // Src always ready
    if (capture->sync_handle) {
        esp_capture_sync_start(capture->sync_handle);
    }
    if (capture->cfg.audio_src && capture->audio_nego_done && capture->fetching_audio == false) {
        if (has_active_path(capture, ESP_CAPTURE_STREAM_TYPE_AUDIO, false) == false) {
            return ESP_CAPTURE_ERR_OK;
        }
        // Try to get audio frame samples firstly
        capture_update_audio_frame_samples(capture);
        // Start audio src
        capture->fetching_audio = true;
        ret = capture->cfg.audio_src->start(capture->cfg.audio_src);
        if (ret != 0) {
            ESP_LOGE(TAG, "Failed to start audio src");
            // TODO send empty data to capture path to let acquire frame quit??
            capture->fetching_audio = false;
        } else {
            // Create src thread to receive data
            ret = media_lib_thread_create_from_scheduler(&handle, "AUD_SRC", audio_src_thread, capture);
            if (ret != 0) {
                ESP_LOGE(TAG, "Failed to create audio src thread");
                capture->fetching_audio = false;
            }
        }
    }
    if (capture->cfg.video_src && capture->video_nego_done && capture->fetching_video == false) {
        if (has_active_path(capture, ESP_CAPTURE_STREAM_TYPE_VIDEO, false) == false) {
            return ESP_CAPTURE_ERR_OK;
        }
        capture->fetching_video = true;
        ret = capture->cfg.video_src->start(capture->cfg.video_src);
        if (ret != 0) {
            ESP_LOGE(TAG, "Failed to start video src");
            // TODO send empty data to capture path to let acquire frame quit??
            capture->fetching_video = false;
        } else {
            ret = media_lib_thread_create_from_scheduler(&handle, "VID_SRC", video_src_thread, capture);
            if (ret != 0) {
                ESP_LOGE(TAG, "Failed to create video src thread");
                capture->fetching_video = false;
            }
        }
    }
    return (capture->fetching_video || capture->fetching_audio) ? ESP_CAPTURE_ERR_OK : ESP_CAPTURE_ERR_NO_RESOURCES;
}

static void capture_send_src_leave_data(capture_t *capture)
{
    if (capture->fetching_audio && capture->audio_src_q && has_active_path(capture, ESP_CAPTURE_STREAM_TYPE_VIDEO, true) && data_queue_have_data(capture->audio_src_q) == false) {
        int frame_size = sizeof(esp_capture_stream_frame_t);
        uint8_t *data = data_queue_get_buffer(capture->audio_src_q, frame_size);
        if (data) {
            esp_capture_stream_frame_t *frame = (esp_capture_stream_frame_t *)data;
            memset(data, 0, sizeof(esp_capture_stream_frame_t));
            frame->stream_type = STOP_CMD_STREAM_TYPE;
            data_queue_send_buffer(capture->audio_src_q, frame_size);
        }
    }
    if (capture->fetching_video && capture->video_src_q && msg_q_number(capture->video_src_q) == 0 && has_active_path(capture, ESP_CAPTURE_STREAM_TYPE_VIDEO, true)) {
        esp_capture_stream_frame_t frame = {
            .stream_type = STOP_CMD_STREAM_TYPE,
        };
        msg_q_send(capture->video_src_q, &frame, sizeof(esp_capture_stream_frame_t));
    }
}

static void capture_stop_src(capture_t *capture)
{
    bool fetching_video = capture->fetching_video;
    bool fetching_audio = capture->fetching_audio;
    capture->fetching_audio = false;
    capture->fetching_video = false;
    // Wait for fetch thread quit
    if (fetching_video) {
        consume_all_video_src(capture);
        media_lib_event_group_wait_bits(capture->event_group, EVENT_GROUP_VIDEO_SRC_EXITED, 1000);
        media_lib_event_group_clr_bits(capture->event_group, EVENT_GROUP_VIDEO_SRC_EXITED);
        // Consume again to flush all input data
        consume_all_video_src(capture);
        capture->cfg.video_src->stop(capture->cfg.video_src);
    }
    if (fetching_audio) {
        consume_all_audio_src(capture);
        media_lib_event_group_wait_bits(capture->event_group, EVENT_GROUP_AUDIO_SRC_EXITED, 1000);
        media_lib_event_group_clr_bits(capture->event_group, EVENT_GROUP_AUDIO_SRC_EXITED);
        consume_all_audio_src(capture);
        capture->cfg.audio_src->stop(capture->cfg.audio_src);
    }
}

int esp_capture_open(esp_capture_cfg_t *cfg, esp_capture_handle_t *h)
{
    if (cfg == NULL || h == NULL || (cfg->audio_src == NULL && cfg->video_src == NULL)) {
        ESP_LOGE(TAG, "Invalid argument cfg:%p capture:%p audio src:%p video_src:%p",
                 cfg, h, cfg ? cfg->audio_src : NULL, cfg ? cfg->video_src : NULL);
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    capture_t *capture = media_lib_calloc(1, sizeof(capture_t));
    if (capture == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for capture");
        return ESP_CAPTURE_ERR_NO_MEM;
    }
    do {
        media_lib_event_group_create(&capture->event_group);
        if (capture->event_group == NULL) {
            break;
        }
        media_lib_mutex_create(&capture->api_lock);
        if (capture->api_lock == NULL) {
            break;
        }
        if (cfg->sync_mode != ESP_CAPTURE_SYNC_MODE_NONE) {
            esp_capture_sync_create(cfg->sync_mode, &capture->sync_handle);
        }
        if (cfg->capture_path) {
            esp_capture_path_cfg_t path_cfg = {
                .acquire_src_frame = capture_path_acquire_frame,
                .release_src_frame = capture_path_release_frame,
                .src_ctx = capture,
                .nego_audio = capture_path_nego_audio_caps,
                .nego_video = capture_path_nego_video_caps,
                .frame_processed = capture_frame_processed,
                .event_cb = capture_path_event_reached,
            };
            if (cfg->capture_path->open(cfg->capture_path, &path_cfg) != ESP_CAPTURE_ERR_OK) {
                ESP_LOGE(TAG, "Failed to open capture path");
                break;
            }
        }
        int ret = ESP_CAPTURE_ERR_OK;
        if (cfg->audio_src) {
            ret = cfg->audio_src->open(cfg->audio_src);
            if (ret != ESP_CAPTURE_ERR_OK) {
                ESP_LOGE(TAG, "Failed to open audio source");
                break;
            }
        }
        if (cfg->video_src) {
            cfg->video_src->open(cfg->video_src);
            if (ret != ESP_CAPTURE_ERR_OK) {
                ESP_LOGE(TAG, "Failed to open video source");
                break;
            }
        }
        capture->cfg = *cfg;
        *h = capture;
        return ESP_CAPTURE_ERR_OK;
    } while (0);
    esp_capture_close(capture);
    return ESP_CAPTURE_ERR_NO_RESOURCES;
}

int esp_capture_setup_path(esp_capture_handle_t h, esp_capture_path_type_t type, esp_capture_sink_cfg_t *sink_info, esp_capture_path_handle_t *path)
{
    capture_t *capture = (capture_t *)h;
    if (capture == NULL || sink_info == NULL || path == NULL || (sink_info->audio_info.codec == ESP_CAPTURE_CODEC_TYPE_NONE && sink_info->video_info.codec == ESP_CAPTURE_CODEC_TYPE_NONE)) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    media_lib_mutex_lock(capture->api_lock, MEDIA_LIB_MAX_LOCK_TIME);
    int ret = ESP_CAPTURE_ERR_OK;
    do {
        if (capture->path_num >= ESP_CAPTURE_PATH_MAX) {
            ESP_LOGE(TAG, "Only support max path %d", ESP_CAPTURE_PATH_MAX);
            BREAK_SET_RETURN(ESP_CAPTURE_ERR_NOT_ENOUGH);
        }
        if (capture->started) {
            ESP_LOGE(TAG, "Not support add path after started");
            BREAK_SET_RETURN(ESP_CAPTURE_ERR_INVALID_STATE);
        }
        if (capture->cfg.capture_path == NULL) {
            if (capture->path_num) {
                ESP_LOGE(TAG, "Only support one path when path interface not set");
                BREAK_SET_RETURN(ESP_CAPTURE_ERR_NOT_SUPPORTED);
            }
        }
        // Path already added
        if (capture->path[type]) {
            capture_path_t *cur = capture->path[type];
            if (cur->enable && capture->started) {
                ESP_LOGW(TAG, "Not allowed to change sink during running");
                BREAK_SET_RETURN(ESP_CAPTURE_ERR_INVALID_STATE);
            }
            cur->sink_cfg = *sink_info;
            *path = (esp_capture_path_handle_t)cur;
            BREAK_SET_RETURN(ESP_CAPTURE_ERR_OK);
        }
        capture->path[capture->path_num] = (capture_path_t *)media_lib_calloc(1, sizeof(capture_path_t));
        if (path == NULL) {
            BREAK_SET_RETURN(ESP_CAPTURE_ERR_NO_MEM);
        }
        capture_path_t *cur = capture->path[capture->path_num];
        cur->path_type = type;
        cur->sink_cfg = *sink_info;
        cur->parent = capture;

        if (capture->cfg.capture_path == NULL) {
            ret = capture_negotiate_directly(capture, sink_info);
        } else {
            ret = capture->cfg.capture_path->add_path(capture->cfg.capture_path, cur->path_type, sink_info);
        }
        if (ret != ESP_CAPTURE_ERR_OK) {
            media_lib_free(cur);
            BREAK_SET_RETURN(ret);
        }
        capture->path_num++;
        *path = (esp_capture_path_handle_t)cur;
        ret = ESP_CAPTURE_ERR_OK;
    } while (0);
    media_lib_mutex_unlock(capture->api_lock);
    return ret;
}

int esp_capture_add_muxer_to_path(esp_capture_path_handle_t h, esp_capture_muxer_cfg_t *muxer_cfg)
{
    capture_path_t *path = (capture_path_t *)h;

    if (path == NULL || muxer_cfg == NULL || path->parent == NULL || muxer_cfg->muxer_type == ESP_CAPTURE_MUXER_TYPE_NONE) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    capture_t *capture = path->parent;
    int ret = ESP_CAPTURE_ERR_OK;
    media_lib_mutex_lock(capture->api_lock, MEDIA_LIB_MAX_LOCK_TIME);
    do {
        if (capture->started) {
            ESP_LOGE(TAG, "Not support add muxer after started");
            BREAK_SET_RETURN(ESP_CAPTURE_ERR_INVALID_STATE);
        }
        if (path->muxer) {
            ESP_LOGE(TAG, "Muxer already added");
            BREAK_SET_RETURN(ESP_CAPTURE_ERR_INVALID_STATE);
        }
        path->muxer_cfg = *muxer_cfg;
    } while (0);
    media_lib_mutex_unlock(capture->api_lock);
    return ret;
}

int esp_capture_add_overlay_to_path(esp_capture_path_handle_t h, esp_capture_overlay_if_t *overlay)
{
    capture_path_t *path = (capture_path_t *)h;
    if (path == NULL || path->parent == NULL || overlay == NULL) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    capture_t *capture = path->parent;
    media_lib_mutex_lock(capture->api_lock, MEDIA_LIB_MAX_LOCK_TIME);
    int ret = ESP_CAPTURE_ERR_OK;
    if (capture->cfg.capture_path == NULL || capture->cfg.capture_path->add_overlay == NULL) {
        ESP_LOGE(TAG, "Capture path not added, not support overlay");
        ret = ESP_CAPTURE_ERR_NOT_SUPPORTED;
    } else {
        ret = capture->cfg.capture_path->add_overlay(capture->cfg.capture_path, path->path_type, overlay);
    }
    media_lib_mutex_unlock(capture->api_lock);
    return ret;
}

int esp_capture_enable_muxer(esp_capture_path_handle_t h, bool enable)
{
    capture_path_t *path = (capture_path_t *)h;
    if (path == NULL || path->parent == NULL) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    media_lib_mutex_lock(path->parent->api_lock, MEDIA_LIB_MAX_LOCK_TIME);
    int ret = enable_muxer(path, enable);
    media_lib_mutex_unlock(path->parent->api_lock);
    return ret;
}

int esp_capture_enable_overlay(esp_capture_path_handle_t h, bool enable)
{
    capture_path_t *path = (capture_path_t *)h;
    if (path == NULL || path->parent == NULL) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    capture_t *capture = path->parent;
    media_lib_mutex_lock(capture->api_lock, MEDIA_LIB_MAX_LOCK_TIME);
    int ret = ESP_CAPTURE_ERR_OK;
    if (capture->cfg.capture_path == NULL || capture->cfg.capture_path->enable_overlay == NULL) {
        ESP_LOGE(TAG, "Capture path not added, not support overlay");
        ret = ESP_CAPTURE_ERR_NOT_SUPPORTED;
    } else {
        ret = capture->cfg.capture_path->enable_overlay(capture->cfg.capture_path, path->path_type, enable);
    }
    media_lib_mutex_unlock(capture->api_lock);
    return ret;
}

int esp_capture_enable_path(esp_capture_path_handle_t h, esp_capture_run_type_t run_type)
{
    capture_path_t *path = (capture_path_t *)h;
    if (path == NULL || path->parent == NULL) {
        printf("Fail for path:%p\n", path);
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    capture_t *capture = path->parent;
    media_lib_mutex_lock(capture->api_lock, MEDIA_LIB_MAX_LOCK_TIME);
    bool enable = run_type != ESP_CAPTURE_RUN_TYPE_DISABLE;
    if (enable) {
        // Clear capture once finish state, so that src will start to send data
        path->run_finished = false;
    }
    if (path->enable == enable) {
        media_lib_mutex_unlock(capture->api_lock);
        return ESP_CAPTURE_ERR_OK;
    }
    int ret = ESP_CAPTURE_ERR_OK;
    if (enable) {
        path->enable = true;
        path->sink_disabled = false;
        path->run_once = (run_type == ESP_CAPTURE_RUN_TYPE_ONCE);
        // Prepare so that data pushed to path queue
        ret = start_path(path);
    } else {
        // Stop from last to first one src -> path -> muxer
        path->sink_disabled = true;
        stop_muxer(path);
        esp_capture_stream_frame_t frame = { 0 };
        if (path->video_share_q) {
            share_q_recv_all(path->video_share_q, &frame);
        }
        if (path->audio_share_q) {
            share_q_recv_all(path->audio_share_q, &frame);
        }
        // Avoid enable once finished, not enable again
        capture_send_src_leave_data(path->parent);
    }
    if (capture->cfg.capture_path) {
        ret = capture->cfg.capture_path->enable_path(capture->cfg.capture_path, path->path_type, enable);
    }
    path->enable = enable;
    if (enable == false) {
        ret = stop_path(path);
        if (has_active_path(capture, ESP_CAPTURE_STREAM_TYPE_VIDEO, false) == false) {
            capture->audio_nego_done = false;
        }
        if (has_active_path(capture, ESP_CAPTURE_STREAM_TYPE_AUDIO, false) == false) {
            capture->video_nego_done = false;
        }
    } else {
        if (capture->started) {
            ret = capture_start_src(capture);
        }
    }
    media_lib_mutex_unlock(capture->api_lock);
    return ret;
}

int esp_capture_start(esp_capture_handle_t h)
{
    capture_t *capture = (capture_t *)h;
    if (capture == NULL) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    media_lib_mutex_lock(capture->api_lock, MEDIA_LIB_MAX_LOCK_TIME);
    if (capture->started) {
        media_lib_mutex_unlock(capture->api_lock);
        ESP_LOGW(TAG, "Already started");
        return ESP_CAPTURE_ERR_OK;
    }
    int ret = ESP_CAPTURE_ERR_OK;
    capture->started = true;
    for (int i = 0; i < capture->path_num; i++) {
        capture_path_t *path = capture->path[i];
        if (path == NULL) {
            continue;
        }
        // Prepare queues and related resource
        ret = start_path(path);
        if (ret != ESP_CAPTURE_ERR_OK) {
            ESP_LOGE(TAG, "Failed to start capture path %d", i);
            // When fail try to start next path
            continue;
        }
        if (capture->cfg.capture_path) {
            ret = capture->cfg.capture_path->start(capture->cfg.capture_path);
        }
    }
    // Start source when path ready
    ret = capture_start_src(capture);
    media_lib_mutex_unlock(capture->api_lock);
    return ret;
}

int esp_capture_set_path_bitrate(esp_capture_path_handle_t h, esp_capture_stream_type_t stream_type, uint32_t bitrate)
{
    capture_path_t *path = (capture_path_t *)h;
    if (path == NULL || path->parent == NULL) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    capture_t *capture = path->parent;
    media_lib_mutex_lock(capture->api_lock, MEDIA_LIB_MAX_LOCK_TIME);
    if (capture->cfg.capture_path == NULL) {
        ESP_LOGE(TAG, "Capture path not supported");
        media_lib_mutex_unlock(capture->api_lock);
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    esp_capture_path_set_type_t type = ESP_CAPTURE_PATH_SET_TYPE_NONE;
    if (stream_type == ESP_CAPTURE_STREAM_TYPE_VIDEO) {
        type = ESP_CAPTURE_PATH_SET_TYPE_VIDEO_BITRATE;
    } else if (stream_type == ESP_CAPTURE_STREAM_TYPE_VIDEO) {
        type = ESP_CAPTURE_PATH_SET_TYPE_AUDIO_BITRATE;
    }
    int ret = capture->cfg.capture_path->set(capture->cfg.capture_path, path->path_type, type, &bitrate, sizeof(uint32_t));
    media_lib_mutex_unlock(capture->api_lock);
    return ret;
}

int esp_capture_acquire_path_frame(esp_capture_path_handle_t h, esp_capture_stream_frame_t *frame, bool no_wait)
{
    capture_path_t *path = (capture_path_t *)h;
    if (path == NULL || path->parent == NULL || frame == NULL) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    capture_t *capture = path->parent;
    // TODO not add lock user need care the timing
    if (path->enable == false) {
        ESP_LOGE(TAG, "Capture path %d is not enabled", path->path_type);
        return ESP_CAPTURE_ERR_INVALID_STATE;
    }
    int ret = ESP_CAPTURE_ERR_NOT_SUPPORTED;
    switch (frame->stream_type) {
        case ESP_CAPTURE_STREAM_TYPE_VIDEO:
            if (capture->cfg.capture_path == NULL) {
                // Get from src directly
                return capture_path_acquire_frame(capture, frame, no_wait);
            }
            if (path->video_path_disabled) {
                if (path->video_share_q) {
                    share_q_recv_all(path->video_share_q, frame);
                }
                return ESP_CAPTURE_ERR_NOT_FOUND;
            }
            // TODO check whether share q send frame only
            if (path->video_q) {
                ret = msg_q_recv(path->video_q, frame, sizeof(esp_capture_stream_frame_t), no_wait);
                ret = (ret == 0) ? ESP_CAPTURE_ERR_OK : ESP_CAPTURE_ERR_NOT_FOUND;
            }
            break;
        case ESP_CAPTURE_STREAM_TYPE_AUDIO:
            if (capture->cfg.capture_path == NULL) {
                // Get from src directly
                return capture_path_acquire_frame(capture, frame, no_wait);
            }
            if (path->audio_path_disabled) {
                if (path->audio_share_q) {
                    share_q_recv_all(path->audio_share_q, frame);
                }
                return ESP_CAPTURE_ERR_NOT_FOUND;
            }
            if (path->audio_q) {
                ret = msg_q_recv(path->audio_q, frame, sizeof(esp_capture_stream_frame_t), no_wait);
                ret = (ret == 0) ? ESP_CAPTURE_ERR_OK : ESP_CAPTURE_ERR_NOT_FOUND;
            }
            break;
        case ESP_CAPTURE_STREAM_TYPE_MUXER:
            if (path->muxer_enable && path->muxer_data_q != NULL) {
                int size = 0;
                void *data = NULL;
                if (no_wait == false) {
                    data_queue_read_lock(path->muxer_data_q, &data, &size);
                } else if (data_queue_have_data(path->muxer_data_q)) {
                    data_queue_read_lock(path->muxer_data_q, &data, &size);
                }
                frame->size = 0;
                if (data) {
                    frame->pts = *(uint32_t *)data;
                    frame->data = (uint8_t *)data + sizeof(uint32_t);
                    frame->size = size - sizeof(uint32_t);
                }
                ret = data ? ESP_CAPTURE_ERR_OK : ESP_CAPTURE_ERR_NOT_FOUND;
            }
            break;
        default:
            return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    return ret;
}

int esp_capture_release_path_frame(esp_capture_path_handle_t h, esp_capture_stream_frame_t *frame)
{
    capture_path_t *path = (capture_path_t *)h;
    if (path == NULL || path->parent == NULL || frame == NULL) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    // TODO not add lock user need care the timing
    capture_t *capture = path->parent;
    if (path->enable == false) {
        ESP_LOGE(TAG, "Capture path %d is not enabled", path->path_type);
        return ESP_CAPTURE_ERR_INVALID_STATE;
    }
    int ret = ESP_CAPTURE_ERR_OK;
    switch (frame->stream_type) {
        case ESP_CAPTURE_STREAM_TYPE_VIDEO:
            if (capture->cfg.capture_path == NULL) {
                return capture_path_release_frame(capture, frame);
            }
            if (path->video_share_q) {
                share_q_release(path->video_share_q, frame);
            }
            break;
        case ESP_CAPTURE_STREAM_TYPE_AUDIO:
            if (capture->cfg.capture_path == NULL) {
                return capture_path_release_frame(capture, frame);
            }
            if (path->audio_share_q) {
                share_q_release(path->audio_share_q, frame);
            }
            break;
        case ESP_CAPTURE_STREAM_TYPE_MUXER:
            if (path->muxer_enable && path->muxer_data_q != NULL) {
                data_queue_read_unlock(path->muxer_data_q);
            }
            break;
        default:
            return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    return ret;
}

int esp_capture_stop(esp_capture_handle_t h)
{
    capture_t *capture = (capture_t *)h;
    if (capture == NULL) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    media_lib_mutex_lock(capture->api_lock, MEDIA_LIB_MAX_LOCK_TIME);
    if (capture->started == false) {
        media_lib_mutex_unlock(capture->api_lock);
        return ESP_CAPTURE_ERR_INVALID_STATE;
    }
    capture->started = false;
    esp_capture_stream_frame_t frame = { 0 };

    // Stop muxer before path for muxer may still keep capture path data
    for (int i = 0; i < capture->path_num; i++) {
        capture_path_t *path = capture->path[i];
        stop_muxer(path);
    }
    // Receive all output firstly to let capture path quit
    for (int i = 0; i < capture->path_num; i++) {
        capture_path_t *path = capture->path[i];
        // Disable path
        path->sink_disabled = true;
        if (path->video_share_q) {
            share_q_recv_all(path->video_share_q, &frame);
        }
        if (path->audio_share_q) {
            share_q_recv_all(path->audio_share_q, &frame);
        }
    }
    capture_send_src_leave_data(capture);
    // Stop path
    if (capture->cfg.capture_path) {
        capture->cfg.capture_path->stop(capture->cfg.capture_path);
    }
    // Send empty data to let user quit
    for (int i = 0; i < capture->path_num; i++) {
        capture_path_t *path = capture->path[i];
        if (path->video_share_q) {
            share_q_recv_all(path->video_share_q, &frame);
            frame.stream_type = ESP_CAPTURE_STREAM_TYPE_VIDEO;
            share_q_add(path->video_share_q, &frame);
        }
        if (path->audio_share_q) {
            share_q_recv_all(path->audio_share_q, &frame);
            frame.stream_type = ESP_CAPTURE_STREAM_TYPE_AUDIO;
            share_q_add(path->audio_share_q, &frame);
        }
    }
    for (int i = 0; i < capture->path_num; i++) {
        capture_path_t *path = capture->path[i];
        stop_path(path);
        release_path(path);
        path->sink_disabled = false;
    }
    capture_stop_src(capture);
    if (capture->sync_handle) {
        esp_capture_sync_stop(capture->sync_handle);
    }
    // Destroy src resources
    if (capture->audio_src_q) {
        data_queue_deinit(capture->audio_src_q);
        capture->audio_src_q = NULL;
    }
    if (capture->video_src_q) {
        msg_q_destroy(capture->video_src_q);
        capture->video_src_q = NULL;
    }
    capture->audio_frames = 0;
    capture->video_frames = 0;
    media_lib_mutex_unlock(capture->api_lock);
    return ESP_CAPTURE_ERR_OK;
}

int esp_capture_close(esp_capture_handle_t h)
{
    if (h == NULL) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    capture_t *capture = (capture_t *)h;
    esp_capture_stop(h);
    if (capture->cfg.capture_path) {
        capture->cfg.capture_path->close(capture->cfg.capture_path);
    }
    if (capture->cfg.audio_src) {
        capture->cfg.audio_src->close(capture->cfg.audio_src);
    }
    if (capture->cfg.video_src) {
        capture->cfg.video_src->close(capture->cfg.video_src);
    }
    if (capture->event_group) {
        media_lib_event_group_destroy(capture->event_group);
        capture->event_group = NULL;
    }
    if (capture->api_lock) {
        media_lib_mutex_destroy(capture->api_lock);
        capture->api_lock = NULL;
    }
    if (capture->sync_handle) {
        esp_capture_sync_destroy(capture->sync_handle);
        capture->sync_handle = NULL;
    }
    media_lib_free(capture);
    return ESP_CAPTURE_ERR_OK;
}
