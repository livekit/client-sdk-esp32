/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2023 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in
 * which case, it is free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the
 * Software without restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include "media_lib_mem_his.h"
#include "media_lib_mem_trace.h"
#include "media_lib_err.h"
#include "esp_log.h"

#define TAG             "Mem_His"
#define SAVE_SLICE_NUM  (4)
#define MAX_WRITE_RETRY (5)

#undef WRITE_USE_LIBC

#ifndef WRITE_USE_LIBC
#include <fcntl.h>
#include <unistd.h>
#endif

#pragma pack(push)
#pragma pack(1)

typedef struct {
    char     act;
    uint8_t  trace_num;
    uint8_t  flag;
    uint8_t  reserv[1];
    void    *addr;
    uint32_t size;
} malloc_detail_t;

typedef struct {
    char    act;
    uint8_t reserv[1];
    void   *addr;
} free_detail_t;

#pragma pack(pop)

typedef struct {
    media_lib_mutex_handle_t write_mutex;
#ifdef WRITE_USE_LIBC
    FILE     *fp;
#else
    int      fd;
#endif
    bool     running;
    bool     stopping;
    uint8_t *save_cache[SAVE_SLICE_NUM];
    int      cache_size;
    int      cache_filled;
    int      wp;
    int      n;
} save_his_t;

static save_his_t *save_his = NULL;

static inline bool his_wait_for_enough(int size)
{
    save_his_t *his = save_his;
    int retry = MAX_WRITE_RETRY;
    while (retry) {
        if (his->n < SAVE_SLICE_NUM || his->cache_filled + size <= his->cache_size) {
            return true;
        }
        retry--;
        media_lib_thread_sleep(10);
    }
    ESP_LOGE(TAG, "Malloc too frequently over tracing write speed");
    return false;
}

static void his_write(void *buffer, int size)
{
    save_his_t *his = save_his;
    while (size) {
        if (his->n >= SAVE_SLICE_NUM) {
            media_lib_thread_sleep(10);
            continue;
        }
        media_lib_mutex_lock(his->write_mutex, MEDIA_LIB_MAX_LOCK_TIME);
        if (his->cache_filled < his->cache_size) {
            int left = his->cache_size - his->cache_filled;
            int fill = left > size ? size : left;
            memcpy(his->save_cache[his->wp] + his->cache_filled, buffer, fill);
            his->cache_filled += fill;
            if (left > fill) {
                media_lib_mutex_unlock(his->write_mutex);
                break;
            }
            his->cache_filled = 0;
            his->wp = (his->wp + 1) % SAVE_SLICE_NUM;
            his->n++;
            size -= fill;
            buffer += fill;
        }
        media_lib_mutex_unlock(his->write_mutex);
    }
}

static void save_thread(void *arg)
{
    save_his_t *his = save_his;
    his->running = true;
    while (1) {
        if (his->n) {
            media_lib_mutex_lock(his->write_mutex, MEDIA_LIB_MAX_LOCK_TIME);
            int head = (his->wp + SAVE_SLICE_NUM - his->n) % SAVE_SLICE_NUM;
            media_lib_mutex_unlock(his->write_mutex);
#ifdef WRITE_USE_LIBC
            fwrite(his->save_cache[head], his->cache_size, 1, his->fp);
#else
            write(his->fd, his->save_cache[head], his->cache_size);
#endif
            media_lib_mutex_lock(his->write_mutex, MEDIA_LIB_MAX_LOCK_TIME);
            his->n--;
            media_lib_mutex_unlock(his->write_mutex);
            continue;
        }
        if (his->stopping) {
            break;
        }
        media_lib_thread_sleep(10);
    }
    if (his->cache_filled) {
#ifdef WRITE_USE_LIBC
        fwrite(his->save_cache[his->wp], his->cache_filled, 1, his->fp);
#else
        write(his->fd, his->save_cache[his->wp], his->cache_filled);
#endif
    }
#ifdef WRITE_USE_LIBC
    if (save_his->fp) {
        fclose(save_his->fp);
        save_his->fp = NULL;
    }
#else
    if (save_his->fd > 0) {
        close(save_his->fd);
        save_his->fd = 0;
    }
#endif
    ESP_LOGI(TAG, "Sync write left %d done", his->cache_filled);
    his->stopping = false;
    his->running = false;
    media_lib_thread_destroy(NULL);
}

static void sync_mem_his(void)
{
    save_his_t *his = save_his;
    his->stopping = true;
    ESP_LOGI(TAG, "waiting for write quit");
    while (his->running) {
        media_lib_thread_sleep(10);
    }
}

int media_lib_start_mem_his(media_lib_mem_trace_cfg_t *cfg)
{
    int ret = ESP_MEDIA_ERR_FAIL;
    do {
        if (save_his) {
            return ESP_MEDIA_ERR_OK;
        }
        save_his = (save_his_t *) calloc(1, sizeof(save_his_t));
        if (save_his == NULL) {
            ret = ESP_MEDIA_ERR_NO_MEM;
            break;
        }
        if (media_lib_mutex_create(&save_his->write_mutex) != ESP_MEDIA_ERR_OK) {
            break;
        }
        const char *file = cfg->save_path ? cfg->save_path : MEDIA_LIB_DEFAULT_SAVE_PATH;
#ifdef WRITE_USE_LIBC
        save_his->fp = fopen(file, "wb");
        if (save_his->fp == NULL) {
            ESP_LOGE(TAG, "Fail to open file %s", file);
            break;
        }
#else
        save_his->fd = open(file, O_WRONLY | O_CREAT | O_TRUNC);
        if (save_his->fd <= 0) {
            ESP_LOGE(TAG, "Fail to open file %s", file);
            break;
        }
#endif
        int size = cfg->save_cache_size ? cfg->save_cache_size : MEDIA_LIB_DEFAULT_SAVE_CACHE_SIZE;
        int each_size = size / SAVE_SLICE_NUM;
        size = each_size * SAVE_SLICE_NUM;
        save_his->save_cache[0] = (uint8_t *) media_lib_malloc(size);
        if (save_his->save_cache[0] == NULL) {
            ESP_LOGE(TAG, "Fail to allocate for save history");
            ret = ESP_MEDIA_ERR_NO_MEM;
            break;
        }
        for (int i = 1; i < SAVE_SLICE_NUM; i++) {
            save_his->save_cache[i] = save_his->save_cache[i - 1] + each_size;
        }
        save_his->cache_size = each_size;
        media_lib_thread_handle_t h;
        if (media_lib_thread_create_from_scheduler(&h, "MemSave", save_thread, NULL) != ESP_MEDIA_ERR_OK) {
            ESP_LOGE(TAG, "No thread resource");
            break;
        }
        return ESP_MEDIA_ERR_OK;
    } while (0);
    if (save_his) {
        media_lib_stop_mem_his();
    }
    return ret;
}

void media_lib_add_mem_malloc_his(void *addr, int size, int stack_num, void *stack, uint8_t flag)
{
    malloc_detail_t detail;
    detail.flag = flag;
    detail.act = '+';
    detail.addr = addr;
    detail.size = size;
    detail.trace_num = stack_num;
    int frame_size = stack_num * sizeof(void *);
    if (his_wait_for_enough(sizeof(detail) + frame_size)) {
        his_write(&detail, sizeof(detail));
        his_write(stack, frame_size);
    }
}

void media_lib_add_mem_free_his(void *addr)
{
    free_detail_t detail;
    detail.act = '-';
    detail.addr = addr;
    if (his_wait_for_enough(sizeof(detail))) {
        his_write(&detail, sizeof(detail));
    }
}

void media_lib_stop_mem_his(void)
{
    if (save_his == NULL) {
        return;
    }
    sync_mem_his();
    if (save_his->write_mutex) {
        media_lib_mutex_destroy(save_his->write_mutex);
        save_his->write_mutex = NULL;
    }
#ifdef WRITE_USE_LIBC
    if (save_his->fp) {
        fclose(save_his->fp);
        save_his->fp = NULL;
    }
#else
    if (save_his->fd > 0) {
        close(save_his->fd);
        save_his->fd = 0;
    }
#endif
    if (save_his->save_cache[0]) {
        media_lib_free(save_his->save_cache[0]);
        save_his->save_cache[0] = NULL;
    }
    save_his = NULL;
}
