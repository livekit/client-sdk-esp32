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
#include "video_render.h"
#include "media_lib_os.h"

typedef struct {
    video_render_ops_t    render_ops;
    video_render_handle_t render_handle;
    bool                  is_open;
    int                   ref_count;
} video_render_t;

static void try_free(video_render_t *v_render)
{
    if (v_render->ref_count) {
        v_render->ref_count--;
    }
    if (v_render->ref_count == 0) {
        if (v_render->render_handle) {
            v_render->render_ops.close(v_render->render_handle);
            v_render->render_handle = NULL;
        }
        media_lib_free(v_render);
    }
}

video_render_handle_t video_render_alloc_handle(video_render_cfg_t *cfg)
{
    if (cfg == NULL || cfg->ops.open == NULL || cfg->ops.close == NULL) {
        return NULL;
    }
    video_render_t *v_render = (video_render_t *)media_lib_calloc(1, sizeof(video_render_t));
    if (v_render == NULL) {
        return NULL;
    }
    memcpy(&v_render->render_ops, &cfg->ops, sizeof(video_render_ops_t));
    v_render->render_handle = v_render->render_ops.open(cfg->cfg, cfg->cfg_size);
    if (v_render->render_handle) {
        v_render->ref_count++;
        return v_render;
    }
    video_render_free_handle(v_render);
    return NULL;
}

bool video_render_format_supported(video_render_handle_t render, av_render_video_frame_type_t frame_type)
{
    video_render_t *v_render = render;
    if (v_render == NULL || v_render->render_handle == NULL) {
        return false;
    }
    return v_render->render_ops.format_support(v_render->render_handle, frame_type);
}

int video_render_open(video_render_handle_t render, av_render_video_frame_info_t *info)
{
    video_render_t *v_render = render;
    if (info == NULL || v_render == NULL || v_render->render_handle == NULL) {
        return -1;
    }
    int ret = v_render->render_ops.set_frame_info(v_render->render_handle, info);
    if (ret != 0) {
        return ret;
    }
    v_render->is_open = true;
    v_render->ref_count++;
    return ret;
}

int video_render_write(video_render_handle_t render, av_render_video_frame_t *video_data)
{
    video_render_t *v_render = render;
    if (video_data == NULL || v_render == NULL || v_render->is_open == false) {
        return -1;
    }
    return v_render->render_ops.write(v_render->render_handle, video_data);
}

int video_render_get_latency(video_render_handle_t render, uint32_t *latency)
{
    video_render_t *v_render = render;
    if (latency == NULL || v_render == NULL || v_render->is_open == false) {
        return -1;
    }
    return v_render->render_ops.get_latency(v_render->render_handle, latency);
}

int video_render_get_frame_buffer(video_render_handle_t render, av_render_frame_buffer_t *buffer)
{
    video_render_t *v_render = render;
    if (buffer == NULL || v_render == NULL) {
        return -1;
    }
    if (v_render->render_ops.get_frame_info == NULL) {
        return ESP_MEDIA_ERR_NOT_SUPPORT;
    }
    return v_render->render_ops.get_frame_buffer(v_render->render_handle, buffer);
}

int video_render_get_frame_info(video_render_handle_t render, av_render_video_frame_info_t *info)
{
    video_render_t *v_render = render;
    if (info == NULL || v_render == NULL || v_render->is_open == false) {
        return -1;
    }
    return v_render->render_ops.get_frame_info(v_render->render_handle, info);
}

int video_render_close(video_render_handle_t render)
{
    video_render_t *v_render = render;
    if (v_render == NULL || v_render->is_open == false) {
        return -1;
    }
    if (v_render->render_ops.clear) {
        v_render->render_ops.clear(v_render->render_handle);
    }
    v_render->is_open = false;
    try_free(v_render);
    return 0;
}

void video_render_free_handle(video_render_handle_t render)
{
    video_render_t *v_render = render;
    if (v_render == NULL) {
        return;
    }
    try_free(v_render);
}
