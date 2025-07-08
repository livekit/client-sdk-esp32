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
#include "audio_render.h"
#include "media_lib_os.h"

typedef struct {
    audio_render_ops_t    render_ops;
    audio_render_handle_t render_handle;
    bool                  is_open;
    int                   ref_count;
} audio_render_t;

static void try_free(audio_render_t *a_render)
{
    if (a_render->ref_count) {
        a_render->ref_count--;
    }
    if (a_render->ref_count == 0) {
        if (a_render->render_handle) {
            a_render->render_ops.deinit(a_render->render_handle);
            a_render->render_handle = NULL;
        }
        media_lib_free(a_render);
    }
}

audio_render_handle_t audio_render_alloc_handle(audio_render_cfg_t *cfg)
{
    if (cfg == NULL || cfg->ops.open == NULL || cfg->ops.close == NULL) {
        return NULL;
    }
    audio_render_t *a_render = (audio_render_t *)media_lib_calloc(1, sizeof(audio_render_t));
    if (a_render == NULL) {
        return NULL;
    }
    memcpy(&a_render->render_ops, &cfg->ops, sizeof(audio_render_ops_t));
    a_render->render_handle = a_render->render_ops.init(cfg->cfg, cfg->cfg_size);
    if (a_render->render_handle) {
        a_render->ref_count++;
        return a_render;
    }
    audio_render_free_handle(a_render);
    return NULL;
}

int audio_render_open(audio_render_handle_t render, av_render_audio_frame_info_t *info)
{
    audio_render_t *a_render = render;
    if (info == NULL || a_render == NULL || a_render->render_handle == NULL) {
        return -1;
    }
    int ret = a_render->render_ops.open(a_render->render_handle, info);
    if (ret != 0) {
        return ret;
    }
    a_render->is_open = true;
    a_render->ref_count++;
    return ret;
}

int audio_render_write(audio_render_handle_t render, av_render_audio_frame_t *audio_data)
{
    audio_render_t *a_render = render;
    if (audio_data == NULL || a_render == NULL || a_render->is_open == false) {
        return -1;
    }
    return a_render->render_ops.write(a_render->render_handle, audio_data);
}

int audio_render_get_latency(audio_render_handle_t render, uint32_t *latency)
{
    audio_render_t *a_render = render;
    if (latency == NULL || a_render == NULL || a_render->is_open == false) {
        return -1;
    }
    return a_render->render_ops.get_latency(a_render->render_handle, latency);
}

int audio_render_set_speed(audio_render_handle_t render, float speed)
{
    audio_render_t *a_render = render;
    if (a_render == NULL || a_render->is_open == false) {
        return -1;
    }
    return a_render->render_ops.set_speed(a_render->render_handle, speed);
}

int audio_render_get_frame_info(audio_render_handle_t render, av_render_audio_frame_info_t *info)
{
    audio_render_t *a_render = render;
    if (info == NULL || a_render == NULL || a_render->is_open == false) {
        return -1;
    }
    return a_render->render_ops.get_frame_info(a_render->render_handle, info);
}

int audio_render_close(audio_render_handle_t render)
{
    audio_render_t *a_render = render;
    if (a_render == NULL || a_render->is_open == false) {
        return -1;
    }
    a_render->is_open = false;
    int ret = a_render->render_ops.close(a_render->render_handle);
    try_free(a_render);
    return ret;
}

void audio_render_free_handle(audio_render_handle_t render)
{
    audio_render_t *a_render = render;
    if (a_render == NULL) {
        return;
    }
    try_free(a_render);
}
