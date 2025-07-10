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

#include "msg_q.h"
#include "share_q.h"
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int   ref_count;
    void *frame_data;
} share_item_t;

typedef struct {
    msg_q_handle_t q;
    bool           enable;
} share_user_info_t;

// Shared queue structure
typedef struct share_q_t {
    bool               external;
    share_q_cfg_t      cfg;
    share_user_info_t *user_q;
    share_item_t      *items;
    uint8_t            valid_count;
    uint8_t            rp;
    uint8_t            wp;
    pthread_mutex_t    lock;
    pthread_cond_t     cond;
} share_q_t;

share_q_t *share_q_create(share_q_cfg_t *cfg)
{
    if (cfg == NULL) {
        return NULL;
    }
    share_q_t *q = (share_q_t *)calloc(1, sizeof(share_q_t));
    if (q == NULL) {
        return NULL;
    }
    q->cfg = *cfg;
    q->items = (share_item_t *)calloc(cfg->q_count, sizeof(share_item_t));
    q->user_q = (share_user_info_t *)calloc(cfg->user_count, sizeof(share_user_info_t));
    if (q->items == NULL || q->user_q == NULL) {
        goto _exit;
    }
    q->external = cfg->use_external_q;
    if (cfg->use_external_q == false) {
        for (int i = 0; i < cfg->user_count; i++) {
            // TODO default all outports not enabled
            // q->user_q[i].enable = true;
            q->user_q[i].q = msg_q_create(cfg->q_count, cfg->item_size);
            if (q->user_q[i].q == NULL) {
                goto _exit;
            }
        }
        q->valid_count = cfg->user_count;
    }
    pthread_cond_init(&q->cond, NULL);
    q->rp = 0;
    q->wp = 0;
    pthread_mutex_init(&q->lock, NULL);
    return q;
_exit:
    share_q_destroy(q);
    return NULL;
}

int share_q_set_external(share_q_t *q, uint8_t index, msg_q_handle_t handle)
{
    if (q == NULL || index >= q->cfg.user_count || q->cfg.use_external_q == false) {
        return -1;
    }
    pthread_mutex_lock(&q->lock);
    q->user_q[index].q = handle;
    pthread_mutex_unlock(&q->lock);
    return 0;
}
int share_q_enable(share_q_t *q, uint8_t index, bool enable)
{
    if (q == NULL || index >= q->cfg.user_count) {
        return -1;
    }
    pthread_mutex_lock(&q->lock);
    q->user_q[index].enable = enable;
    uint8_t valid_count = 0;
    for (int i = 0; i < q->cfg.user_count; i++) {
        if (q->user_q[i].enable) {
            valid_count++;
        }
    }
    q->valid_count = valid_count;

    // When disable, receive all from queues
    if (enable == false) {
        pthread_mutex_unlock(&q->lock);
        void *frame = calloc(1, q->cfg.item_size);
        if (frame) {
            while (msg_q_recv(q->user_q[index].q, frame, q->cfg.item_size, true) == 0) {
                share_q_release(q, frame);
            }
            free(frame);
        }
        pthread_mutex_lock(&q->lock);
    }
    pthread_mutex_unlock(&q->lock);
    return 0;
}

bool share_q_is_enabled(share_q_handle_t q, uint8_t index)
{
    if (q == NULL || index >= q->cfg.user_count) {
        return false;
    }
    pthread_mutex_lock(&q->lock);
    bool enabled = q->user_q[index].enable;
    pthread_mutex_unlock(&q->lock);
    return enabled;
}

// Get a user queue handle
msg_q_handle_t share_q_get_q(share_q_t *q, uint8_t index)
{
    if (q == NULL || index >= q->cfg.user_count) {
        return NULL;
    }
    return q->user_q[index].q;
}

// Receive a frame from a user queue
int share_q_recv(share_q_t *q, uint8_t index, void *frame)
{
    if (q == NULL || index >= q->cfg.user_count) {
        return -1;
    }
    int ret = msg_q_recv(q->user_q[index].q, frame, q->cfg.item_size, false);
    return ret;
}

int share_q_recv_all(share_q_handle_t q, void *frame)
{
    if (q == NULL || frame == NULL) {
        return -1;
    }
    // pthread_mutex_lock(&q->lock);
    for (int i = 0; i < q->cfg.user_count; i++) {
        if (q->user_q[i].enable) {
            while (msg_q_recv(q->user_q[i].q, frame, q->cfg.item_size, true) == 0) {
                share_q_release(q, frame);
            }
        }
    }
    // pthread_mutex_unlock(&q->lock);
    return 0;
}

// Add an item to the shared queue
int share_q_add(share_q_t *q, void *item)
{
    if (q == NULL || item == NULL) {
        return -1;
    }
    pthread_mutex_lock(&q->lock);
    if (q->valid_count == 0) {
        q->cfg.release_frame(item, q->cfg.ctx);
        pthread_mutex_unlock(&q->lock);
        return 0;
    }
    // Check if the next write position will overwrite an unreleased item
    int next_wp = (q->wp + 1) % q->cfg.q_count;
    while (next_wp == q->rp) {
        // Queue is full, cannot add new item
        pthread_cond_wait(&q->cond, &q->lock);
    }
    // Add into items first
    share_item_t *q_item = q->items + q->wp;
    q_item->frame_data = q->cfg.get_frame_data(item);
    q_item->ref_count = q->valid_count;
    q->wp = next_wp;
    // Add items into user queues
    for (int i = 0; i < q->cfg.user_count; i++) {
        if (q->user_q[i].enable == false || q->user_q[i].q == NULL) {
            continue;
        }
        if (msg_q_send(q->user_q[i].q, item, q->cfg.item_size) != 0) {
            pthread_mutex_unlock(&q->lock);
            return -1;
        }
    }
    pthread_mutex_unlock(&q->lock);
    return 0;
}

// Release an item from the shared queue
int share_q_release(share_q_t *q, void *item)
{
    if (q == NULL || item == NULL) {
        return -1;
    }
    pthread_mutex_lock(&q->lock);
    // Find and decrement reference count
    int rp = q->rp;
    int wp = q->wp;
    void *frame_data = q->cfg.get_frame_data(item);
    while (rp != wp) {
        share_item_t *q_item = &q->items[rp];
        if (q_item->frame_data == frame_data) {
            q_item->ref_count--;
            if (q_item->ref_count == 0) {
                q->cfg.release_frame(item, q->cfg.ctx);
                q->rp = (q->rp + 1) % q->cfg.q_count;
                pthread_cond_signal(&q->cond);
            }
            pthread_mutex_unlock(&q->lock);
            return 0;
        }
        rp = (rp + 1) % (q->cfg.q_count);
    }
    pthread_mutex_unlock(&q->lock);
    printf("Not found frame data in q %p\n", frame_data);
    return -1;
}

void share_q_destroy(share_q_t *q)
{
    if (q == NULL) {
        return;
    }
    if (q->items) {
        free(q->items);
    }
    if (q->user_q) {
        if (q->external == false) {
            for (int i = 0; i < q->cfg.user_count; i++) {
                if (q->user_q[i].q) {
                    msg_q_destroy(q->user_q[i].q);
                }
            }
        }
        free(q->user_q);
    }
    pthread_mutex_destroy(&(q->lock));
    pthread_cond_destroy(&(q->cond));
    free(q);
}