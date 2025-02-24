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

#include "esp_peer.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    esp_peer_ops_t    ops;
    esp_peer_handle_t handle;
} peer_wrapper_t;

int esp_peer_open(esp_peer_cfg_t *cfg, const esp_peer_ops_t *ops, esp_peer_handle_t *handle)
{
    if (cfg == NULL || ops == NULL || handle == NULL || ops->open == NULL) {
        return ESP_PEER_ERR_INVALID_ARG;
    }
    peer_wrapper_t *peer = calloc(1, sizeof(peer_wrapper_t));
    if (peer == NULL) {
        return ESP_PEER_ERR_NO_MEM;
    }
    memcpy(&peer->ops, ops, sizeof(esp_peer_ops_t));
    int ret = ops->open(cfg, &peer->handle);
    if (ret != ESP_PEER_ERR_NONE) {
        free(peer);
        return ret;
    }
    *handle = peer;
    return ret;
}

int esp_peer_new_connection(esp_peer_handle_t handle)
{
    if (handle == NULL) {
        return ESP_PEER_ERR_INVALID_ARG;
    }
    peer_wrapper_t *peer = (peer_wrapper_t *)handle;
    if (peer->ops.new_connection) {
        return peer->ops.new_connection(peer->handle);
    }
    return ESP_PEER_ERR_NOT_SUPPORT;
}

int esp_peer_update_ice_info(esp_peer_handle_t handle, esp_peer_role_t role, esp_peer_ice_server_cfg_t* server, int server_num)
{
    if (handle == NULL) {
        return ESP_PEER_ERR_INVALID_ARG;
    }
    peer_wrapper_t *peer = (peer_wrapper_t *)handle;
    if (peer->ops.update_ice_info) {
        return peer->ops.update_ice_info(peer->handle, role, server, server_num);
    }
    return ESP_PEER_ERR_NOT_SUPPORT;
}

int esp_peer_send_msg(esp_peer_handle_t handle, esp_peer_msg_t *msg)
{
    if (handle == NULL) {
        return ESP_PEER_ERR_INVALID_ARG;
    }
    peer_wrapper_t *peer = (peer_wrapper_t *)handle;
    if (peer->ops.send_msg) {
        return peer->ops.send_msg(peer->handle, msg);
    }
    return ESP_PEER_ERR_NOT_SUPPORT;
}

int esp_peer_send_video(esp_peer_handle_t handle, esp_peer_video_frame_t *info)
{
    if (handle == NULL || info == NULL) {
        return ESP_PEER_ERR_INVALID_ARG;
    }
    peer_wrapper_t *peer = (peer_wrapper_t *)handle;
    if (peer->ops.send_video) {
        return peer->ops.send_video(peer->handle, info);
    }
    return ESP_PEER_ERR_NOT_SUPPORT;
}

int esp_peer_send_audio(esp_peer_handle_t handle, esp_peer_audio_frame_t *info)
{
    if (handle == NULL || info == NULL) {
        return ESP_PEER_ERR_INVALID_ARG;
    }
    peer_wrapper_t *peer = (peer_wrapper_t *)handle;
    if (peer->ops.send_audio) {
        return peer->ops.send_audio(peer->handle, info);
    }
    return ESP_PEER_ERR_NOT_SUPPORT;
}

int esp_peer_send_data(esp_peer_handle_t handle, esp_peer_data_frame_t *info)
{
    if (handle == NULL || info == NULL) {
        return ESP_PEER_ERR_INVALID_ARG;
    }
    peer_wrapper_t *peer = (peer_wrapper_t *)handle;
    if (peer->ops.send_data) {
        return peer->ops.send_data(peer->handle, info);
    }
    return ESP_PEER_ERR_NOT_SUPPORT;
}

int esp_peer_main_loop(esp_peer_handle_t handle)
{
    if (handle == NULL) {
        return ESP_PEER_ERR_INVALID_ARG;
    }
    peer_wrapper_t *peer = (peer_wrapper_t *)handle;
    if (peer->ops.main_loop) {
        return peer->ops.main_loop(peer->handle);
    }
    return ESP_PEER_ERR_NOT_SUPPORT;
}

int esp_peer_disconnect(esp_peer_handle_t handle)
{
    if (handle == NULL) {
        return ESP_PEER_ERR_INVALID_ARG;
    }
    peer_wrapper_t *peer = (peer_wrapper_t *)handle;
    if (peer->ops.disconnect) {
        return peer->ops.disconnect(peer->handle);
    }
    return ESP_PEER_ERR_NOT_SUPPORT;
}

int esp_peer_query(esp_peer_handle_t handle)
{
    if (handle == NULL) {
        return ESP_PEER_ERR_INVALID_ARG;
    }
    peer_wrapper_t *peer = (peer_wrapper_t *)handle;
    if (peer->ops.query) {
        peer->ops.query(peer->handle);
        return ESP_PEER_ERR_NONE;
    }
    return ESP_PEER_ERR_NOT_SUPPORT;
}

int esp_peer_close(esp_peer_handle_t handle)
{
    if (handle == NULL) {
        return ESP_PEER_ERR_INVALID_ARG;
    }
    peer_wrapper_t *peer = (peer_wrapper_t *)handle;
    int ret = ESP_PEER_ERR_NOT_SUPPORT;
    if (peer->ops.close) {
        ret = peer->ops.close(peer->handle);
    }
    free(peer);
    return ret;
}
