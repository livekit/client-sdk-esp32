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
#if CONFIG_IDF_TARGET_ESP32P4
#include "esp_capture_types.h"
#include "linux/videodev2.h"
#include "esp_video_init.h"
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <string.h>
#include "esp_capture_video_src_if.h"
#include "esp_capture_defaults.h"
#include "esp_cache.h"
#include "esp_log.h"

#define TAG "V4L2_SRC"

#define MAX_SUPPORT_FORMATS_NUM (4)
#define FMT_STR(fmt)            ((uint8_t *)&fmt)[0], ((uint8_t *)&fmt)[1], ((uint8_t *)&fmt)[2], ((uint8_t *)&fmt)[3]

typedef struct {
    esp_capture_video_src_if_t base;
    char                       dev_name[16];
    uint8_t                    buf_count;
    esp_capture_codec_type_t   support_formats[MAX_SUPPORT_FORMATS_NUM];
    uint8_t                    format_count;
    int                        fd;
    uint8_t                   *fb_buffer[2];
    struct v4l2_buffer         v4l2_buf[2];
    bool                       fb_used[2];
    bool                       nego_ok;
    bool                       started;
} v4l2_src_t;

static esp_capture_codec_type_t get_codec_type(uint32_t fmt)
{
    switch (fmt) {
        case V4L2_PIX_FMT_RGB565:
            return ESP_CAPTURE_CODEC_TYPE_RGB565;
        case V4L2_PIX_FMT_YUV420:
            return ESP_CAPTURE_CODEC_TYPE_YUV420;
        case V4L2_PIX_FMT_YUV422P:
            return ESP_CAPTURE_CODEC_TYPE_YUV422P;
        case V4L2_PIX_FMT_MJPEG:
        case V4L2_PIX_FMT_JPEG:
            return ESP_CAPTURE_CODEC_TYPE_MJPEG;
        default:
            return ESP_CAPTURE_CODEC_TYPE_NONE;
    }
}

static uint32_t get_v4l2_type(esp_capture_codec_type_t codec)
{
    switch (codec) {
        case ESP_CAPTURE_CODEC_TYPE_YUV420:
            return V4L2_PIX_FMT_YUV420;
        case ESP_CAPTURE_CODEC_TYPE_YUV422P:
            return V4L2_PIX_FMT_YUV422P;
        case ESP_CAPTURE_CODEC_TYPE_MJPEG:
            return V4L2_PIX_FMT_MJPEG;
        case ESP_CAPTURE_CODEC_TYPE_RGB565:
            return V4L2_PIX_FMT_RGB565;
        default:
            return 0;
    }
}

static int v4l2_open(esp_capture_video_src_if_t *src)
{
    v4l2_src_t *v4l2 = (v4l2_src_t *)src;
    do {
        struct v4l2_capability capability;
        v4l2->fd = open(v4l2->dev_name, O_RDONLY);
        if (v4l2->fd <= 0) {
            ESP_LOGE(TAG, "failed to open device");
            return ESP_FAIL;
        }
        if (ioctl(v4l2->fd, VIDIOC_QUERYCAP, &capability)) {
            ESP_LOGE(TAG, "failed to get capability");
            break;
        }
        if ((capability.capabilities & V4L2_CAP_VIDEO_CAPTURE) != V4L2_CAP_VIDEO_CAPTURE) {
            ESP_LOGE(TAG, "Not support capture");
            break;
        }
        v4l2->format_count = 0;
        for (int i = 0; i < MAX_SUPPORT_FORMATS_NUM; i++) {
            struct v4l2_fmtdesc fmtdesc = {
                .index = i,
                .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            };
            if (ioctl(v4l2->fd, VIDIOC_ENUM_FMT, &fmtdesc) != 0) {
                break;
            }
            v4l2->support_formats[i] = get_codec_type(fmtdesc.pixelformat);
            v4l2->format_count++;
            ESP_LOGI(TAG, "Support Format: %c%c%c%c", FMT_STR(fmtdesc.pixelformat));
        }
        if (v4l2->format_count == 0) {
            ESP_LOGE(TAG, "No support format");
            break;
        }
        ESP_LOGI(TAG, "Success to open camera");
        return 0;
    } while (0);
    if (v4l2->fd > 0) {
        close(v4l2->fd);
        v4l2->fd = 0;
    }
    return -1;
}

static int v4l2_get_support_codecs(esp_capture_video_src_if_t *src, const esp_capture_codec_type_t **codecs,
                                   uint8_t *num)
{
    v4l2_src_t *v4l2 = (v4l2_src_t *)src;
    *codecs = v4l2->support_formats;
    *num = v4l2->format_count;
    return 0;
}

static bool v4l2_is_input_supported(v4l2_src_t *v4l2, esp_capture_codec_type_t in_codec)
{
    for (uint8_t i = 0; i < v4l2->format_count; i++) {
        if (v4l2->support_formats[i] == in_codec) {
            return true;
        }
    }
    return false;
}

static int v4l2_negotiate_caps(esp_capture_video_src_if_t *src, esp_capture_video_info_t *in_cap,
                               esp_capture_video_info_t *out_caps)
{
    v4l2_src_t *v4l2 = (v4l2_src_t *)src;
    if (v4l2_is_input_supported(v4l2, in_cap->codec) == false) {
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    if (v4l2->nego_ok) {
        *out_caps = *in_cap;
        return ESP_CAPTURE_ERR_OK;
    }
    do {
        struct v4l2_requestbuffers req = { 0 };
        struct v4l2_format format = {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .fmt.pix.width = in_cap->width,
            .fmt.pix.height = in_cap->height,
        };
        int ret = 0;
        format.fmt.pix.pixelformat = get_v4l2_type(in_cap->codec);
        if ((ret = ioctl(v4l2->fd, VIDIOC_S_FMT, &format)) != 0) {
            ESP_LOGE(TAG, "failed to set format codec %d %x ret %d", (int)in_cap->codec, (int)format.fmt.pix.pixelformat, ret);
            break;
        }
        req.count = v4l2->buf_count;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
        if (ioctl(v4l2->fd, VIDIOC_REQBUFS, &req) != 0) {
            ESP_LOGE(TAG, "failed to require buffer");
            break;
        }
        for (int i = 0; i < v4l2->buf_count; i++) {
            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            if (ioctl(v4l2->fd, VIDIOC_QUERYBUF, &buf) != 0) {
                break;
            }
            v4l2->fb_buffer[i] = (uint8_t *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, v4l2->fd, buf.m.offset);
            if (!v4l2->fb_buffer[i]) {
                ESP_LOGE(TAG, "failed to map buffer");
                break;
            }
            if (ioctl(v4l2->fd, VIDIOC_QBUF, &buf) != 0) {
                ESP_LOGE(TAG, "failed to queue video frame");
                break;
            }
        }
        *out_caps = *in_cap;
        v4l2->nego_ok = true;
        return ESP_CAPTURE_ERR_OK;
    } while (0);
    return ESP_CAPTURE_ERR_NOT_SUPPORTED;
}

static int v4l2_start(esp_capture_video_src_if_t *src)
{
    v4l2_src_t *v4l2 = (v4l2_src_t *)src;
    if (v4l2 == NULL) {
        return -1;
    }
    if (v4l2->nego_ok == false) {
        ESP_LOGE(TAG, "negotiation not all right");
        return -1;
    }
    uint32_t type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(v4l2->fd, VIDIOC_STREAMON, &type);
    v4l2->started = true;
    return 0;
}

static int v4l2_acquire_frame(esp_capture_video_src_if_t *src, esp_capture_stream_frame_t *frame)
{
    v4l2_src_t *v4l2 = (v4l2_src_t *)src;
    if (v4l2->started == false) {
        return -1;
    }
    struct v4l2_buffer buf = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_MMAP,
    };
    int ret = ioctl(v4l2->fd, VIDIOC_DQBUF, &buf);
    if (ret != 0) {
        ESP_LOGE(TAG, "failed to receive video frame ret %d", ret);
        return -1;
    }
    v4l2->fb_used[buf.index] = true;
    frame->data = v4l2->fb_buffer[buf.index];
    frame->size = buf.bytesused;
    v4l2->v4l2_buf[buf.index] = buf;
    esp_cache_msync(frame->data, frame->size, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
    return 0;
}

static int v4l2_release_frame(esp_capture_video_src_if_t *src, esp_capture_stream_frame_t *frame)
{
    v4l2_src_t *v4l2 = (v4l2_src_t *)src;
    if (v4l2 == NULL) {
        return -1;
    }
    if (v4l2->started == false) {
        return -1;
    }
    for (int i = 0; i < v4l2->buf_count; i++) {
        struct v4l2_buffer *buf = &v4l2->v4l2_buf[i];
        if (v4l2->fb_used[i] && v4l2->fb_buffer[i] == frame->data) {
            v4l2->fb_used[i] = 0;
            ioctl(v4l2->fd, VIDIOC_QBUF, buf);
            return 0;
        }
    }
    ESP_LOGW(TAG, "not found frame %p", frame->data);
    return -1;
}

static int v4l2_stop(esp_capture_video_src_if_t *src)
{
    v4l2_src_t *v4l2 = (v4l2_src_t *)src;
    if (v4l2 == NULL) {
        return -1;
    }
    uint32_t type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(v4l2->fd, VIDIOC_STREAMOFF, &type);
    v4l2->nego_ok = false;
    return 0;
}

static int v4l2_close(esp_capture_video_src_if_t *src)
{
    v4l2_src_t *v4l2 = (v4l2_src_t *)src;
    if (v4l2 == NULL) {
        return -1;
    }
    for (int i = 0; i < v4l2->buf_count; i++) {
        struct v4l2_buffer *buf = &v4l2->v4l2_buf[i];
        if (v4l2->fb_used[i]) {
            v4l2->fb_used[i] = 0;
            ioctl(v4l2->fd, VIDIOC_QBUF, buf);
        }
    }
    if (v4l2->fd > 0) {
        close(v4l2->fd);
    }
    v4l2->fd = 0;
    return 0;
}

esp_capture_video_src_if_t *esp_capture_new_video_v4l2_src(esp_capture_video_v4l2_src_cfg_t *cfg)
{
    if (cfg == NULL || cfg->buf_count == 0) {
        return NULL;
    }
    v4l2_src_t *v4l2 = calloc(1, sizeof(v4l2_src_t));
    if (v4l2 == NULL) {
        return NULL;
    }
    v4l2->base.open = v4l2_open;
    v4l2->base.get_support_codecs = v4l2_get_support_codecs;
    v4l2->base.negotiate_caps = v4l2_negotiate_caps;
    v4l2->base.start = v4l2_start;
    v4l2->base.acquire_frame = v4l2_acquire_frame;
    v4l2->base.release_frame = v4l2_release_frame;
    v4l2->base.stop = v4l2_stop;
    v4l2->base.close = v4l2_close;
    strncpy(v4l2->dev_name, cfg->dev_name, sizeof(v4l2->dev_name));
    // TODO limit to 2
    v4l2->buf_count = cfg->buf_count > 2 ? 2 : cfg->buf_count;
    return &v4l2->base;
}
#endif
