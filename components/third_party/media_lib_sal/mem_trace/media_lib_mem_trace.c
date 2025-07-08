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
#include <string.h>
#include "media_lib_mem_trace.h"
#include "media_lib_mem_his.h"
#include "media_lib_err.h"
#include "esp_log.h"

#define TAG             "Mem_Trace"
#define MAX_STACK_DEPTH (10)

typedef struct {
    void   *addr;
    int     size;
    uint8_t depth;
    uint8_t module_id;
    void   *stack[0];
} mem_trace_item_t;

typedef struct _module_mem_info {
    char                    *module;
    uint8_t                  module_id;
    uint32_t                 mem_usage;
    uint32_t                 peak_mem_usage;
    struct _module_mem_info *next;
} module_mem_info_t;

typedef struct {
    mem_trace_item_t        *trace_item;
    module_mem_info_t       *module_lists;
    uint16_t                 module_num;
    uint16_t                 trace_item_num;
    int                      item_size;
    uint32_t                 mem_usage;
    uint32_t                 peak_mem_usage;
    media_lib_mem_t          kept;
    bool                     overflow;
    media_lib_mutex_handle_t mutex;
} mem_trace_t;

static media_lib_mem_trace_cfg_t trace_cfg;
static mem_trace_t *mem_trace;

static module_mem_info_t *get_module(const char *name)
{
    if (name == NULL) {
        return NULL;
    }
    module_mem_info_t *iter = mem_trace->module_lists;
    while (iter) {
        if (iter->module == name || strcmp(name, iter->module) == 0) {
            return iter;
        }
        iter = iter->next;
    }
    return NULL;
}

static module_mem_info_t *get_module_by_id(uint8_t module_id)
{
    module_mem_info_t *iter = mem_trace->module_lists;
    while (iter) {
        if (iter->module_id == module_id) {
            return iter;
        }
        iter = iter->next;
    }
    return NULL;
}

static module_mem_info_t *alloc_module(const char *name)
{
    if (mem_trace->module_num > 0xFF) {
        ESP_LOGE(TAG, "Too many modules, max support 255");
        return NULL;
    }
    module_mem_info_t *m = (module_mem_info_t *) mem_trace->kept.calloc(1, sizeof(module_mem_info_t));
    if (m == NULL) {
        return NULL;
    }
    m->module = mem_trace->kept.strdup(name);
    if (m->module == NULL) {
        mem_trace->kept.free(m);
        return NULL;
    }
    m->module_id = (uint8_t) mem_trace->module_num;
    mem_trace->module_num++;
    // Insert into module lists
    if (mem_trace->module_lists == NULL) {
        mem_trace->module_lists = m;
    } else {
        module_mem_info_t *tail = mem_trace->module_lists;
        while (tail) {
            if (tail->next == NULL) {
                tail->next = m;
                break;
            }
            tail = tail->next;
        }
    }
    return m;
}

static void free_module(void)
{
    module_mem_info_t *iter = mem_trace->module_lists;
    while (iter) {
        module_mem_info_t *nxt = iter->next;
        if (iter->module) {
            mem_trace->kept.free(iter->module);
        }
        mem_trace->kept.free(iter);
        iter = nxt;
    }
}

static void print_mem_usage(const char *module)
{
    module_mem_info_t *m = get_module(module);
    if (m == NULL) {
        ESP_LOGI(TAG, "Total unfree: %d peak usage: %d", (int) mem_trace->mem_usage, (int) mem_trace->peak_mem_usage);
    } else {
        ESP_LOGI(TAG, "Module %s unfree: %d peak usage: %d", module, (int) mem_trace->mem_usage,
                 (int) mem_trace->peak_mem_usage);
    }
}

static int print_leak(const char *module)
{
    module_mem_info_t *m = get_module(module);
    if (m) {
        ESP_LOGI(TAG, "Leakage module:%s", module);
    }
    int leak_size = 0;
    void *trace_arr = mem_trace->trace_item;
    int item_size = mem_trace->item_size;
    for (int i = 0; i < mem_trace->trace_item_num; i++) {
        // TODO correct print
        mem_trace_item_t *item = (mem_trace_item_t *) trace_arr;
        if (m == NULL || m->module_id == item->module_id) {
            printf("%p size: %d\n", item->addr, item->size);
            for (int d = 0; d < item->depth; d++) {
                printf("%p:", item->stack[d]);
            }
            printf("\n");
            leak_size += item->size;
        }
        trace_arr += item_size;
    }
    printf("total leakage: %d\n", leak_size);
    return leak_size;
}

static uint8_t add_mem_usage(const char *module, int size)
{
    mem_trace->mem_usage += size;
    if (mem_trace->peak_mem_usage < mem_trace->mem_usage) {
        mem_trace->peak_mem_usage = mem_trace->mem_usage;
    }
    if ((trace_cfg.trace_type & MEDIA_LIB_MEM_TRACE_MODULE_USAGE) == 0) {
        return 0;
    }
    module_mem_info_t *m = get_module(module);
    if (module && m == NULL) {
        m = alloc_module(module);
    }
    if (m) {
        m->mem_usage += size;
        if (m->peak_mem_usage < m->mem_usage) {
            m->peak_mem_usage = m->mem_usage;
        }
        return m->module_id;
    }
    return 0;
}

static void remove_mem_usage(uint8_t module_id, int size)
{
    mem_trace->mem_usage -= size;
    if ((trace_cfg.trace_type & MEDIA_LIB_MEM_TRACE_MODULE_USAGE) == 0) {
        return;
    }
    module_mem_info_t *m = get_module_by_id(module_id);
    if (m) {
        m->mem_usage -= size;
    }
}

static mem_trace_item_t *get_trace_item(void *addr)
{
    if (mem_trace->trace_item_num == 0) {
        return NULL;
    }
    void *trace_arr = mem_trace->trace_item;
    for (int i = 0; i < mem_trace->trace_item_num; i++) {
        mem_trace_item_t *item = (mem_trace_item_t *) trace_arr;
        if (addr == item->addr) {
            return item;
        }
        trace_arr += mem_trace->item_size;
    }
    return NULL;
}

static void add_trace_item(uint8_t module_id, void *ptr, int size, void **stack, int depth)
{
    if (mem_trace->trace_item_num >= trace_cfg.record_num) {
        if (mem_trace->overflow == false) {
            mem_trace->overflow = true;
            ESP_LOGE(TAG, "Trace overflow %d > %d", mem_trace->trace_item_num, trace_cfg.record_num);
        }
        return;
    }
    if (size == 860) {
        *(int*)0 = 0;
    }
    mem_trace->overflow = false;
    void *trace_arr = mem_trace->trace_item;
    int item_size = mem_trace->item_size * mem_trace->trace_item_num;
    mem_trace_item_t *item = (mem_trace_item_t *) (trace_arr + item_size);
    item->module_id = module_id;
    item->addr = ptr;
    item->size = size;
    item->depth = depth;
    if (depth) {
        memcpy(item->stack, (void *) stack, depth * sizeof(void *));
    }
    if (size == 860) {
        printf("%p size: %d\n", item->addr, item->size);
        for (int d = 0; d < item->depth; d++) {
            printf("%p:", item->stack[d]);
        }
    }
    mem_trace->trace_item_num++;
}

static void remove_trace_item(mem_trace_item_t *item)
{
    void *trace_arr = mem_trace->trace_item;
    int item_size = mem_trace->item_size;
    if (mem_trace->trace_item_num) {
        mem_trace->trace_item_num--;
        mem_trace_item_t *tail = (mem_trace_item_t *) (trace_arr + mem_trace->trace_item_num * item_size);
        *item = *tail;
        memset(tail, 0, sizeof(mem_trace_item_t));
    }
}

static __attribute__((always_inline)) inline void add_trace(const char *module, void *ptr, int size, uint8_t flag)
{
    media_lib_mutex_lock(mem_trace->mutex, MEDIA_LIB_MAX_LOCK_TIME);
    uint8_t module_id = add_mem_usage(module, size);
    int n = trace_cfg.stack_depth;
    void *stack[MAX_STACK_DEPTH];

    if (n) {
        if (trace_cfg.trace_type & (MEDIA_LIB_MEM_TRACE_SAVE_HISTORY | MEDIA_LIB_MEM_TRACE_LEAK)) {
            n = mem_trace->kept.get_stack_frame(stack, n);
        } else {
            n = 0;
        }
    }
    if (trace_cfg.trace_type & MEDIA_LIB_MEM_TRACE_SAVE_HISTORY) {
        media_lib_add_mem_malloc_his(ptr, size, n, stack, flag);
    }
    if (trace_cfg.record_num) {
        add_trace_item(module_id, ptr, size, stack, n);
    }
    media_lib_mutex_unlock(mem_trace->mutex);
}

static __attribute__((always_inline)) inline void remove_trace(void *ptr)
{
    media_lib_mutex_lock(mem_trace->mutex, MEDIA_LIB_MAX_LOCK_TIME);
    if (trace_cfg.trace_type & MEDIA_LIB_MEM_TRACE_SAVE_HISTORY) {
        media_lib_add_mem_free_his(ptr);
    }
    mem_trace_item_t *item = get_trace_item(ptr);
    if (item) {
        remove_mem_usage(item->module_id, item->size);
        remove_trace_item(item);
    }
    media_lib_mutex_unlock(mem_trace->mutex);
}

static void *_malloc(size_t size)
{
    void *ptr = mem_trace->kept.malloc(size);
    if (ptr) {
        add_trace(NULL, ptr, size, 0);
    }
    return ptr;
}

static void *_malloc_align(size_t size, uint8_t align)
{
    void *ptr = mem_trace->kept.malloc_align(size, align);
    if (ptr) {
        add_trace(NULL, ptr, size, 0);
    }
    return ptr;
}

static void _free_align(void *buf)
{
    remove_trace(buf);
    mem_trace->kept.free_align(buf);
}

static void _free(void *buf)
{
    remove_trace(buf);
    mem_trace->kept.free(buf);
}

static void *_calloc(size_t num, size_t size)
{
    void *ptr = mem_trace->kept.calloc(num, size);
    if (ptr) {
        add_trace(NULL, ptr, num * size, 0);
    }
    return ptr;
}

static void *_realloc(void *buf, size_t size)
{
    void *ptr = mem_trace->kept.realloc(buf, size);
    media_lib_mutex_lock(mem_trace->mutex, MEDIA_LIB_MAX_LOCK_TIME);
    if (buf) {
        remove_trace(buf);
    }
    if (ptr) {
        add_trace(NULL, ptr, size, 0);
    }
    media_lib_mutex_unlock(mem_trace->mutex);
    return ptr;
}

static char *_strdup(const char *str)
{
    char *ptr = mem_trace->kept.strdup(str);
    if (ptr) {
        int len = strlen(ptr) + 1;
        add_trace(NULL, ptr, len, 0);
    }
    return ptr;
}

void *media_lib_module_malloc(const char *module, size_t size)
{
    if (trace_cfg.trace_type == MEDIA_LIB_MEM_TRACE_NONE) {
        return media_lib_malloc(size);
    }
    void *ptr = mem_trace->kept.malloc(size);
    if (ptr) {
        add_trace(module, ptr, size, 0);
    }
    return ptr;
}

void *media_lib_module_calloc(const char *module, size_t num, size_t size)
{
    if (trace_cfg.trace_type == MEDIA_LIB_MEM_TRACE_NONE) {
        return media_lib_calloc(num, size);
    }
    void *ptr = mem_trace->kept.calloc(num, size);
    if (ptr) {
        add_trace(module, ptr, num * size, 0);
    }
    return ptr;
}

void *media_lib_module_realloc(const char *module, void *buf, size_t size)
{
    if (trace_cfg.trace_type == MEDIA_LIB_MEM_TRACE_NONE) {
        return media_lib_realloc(buf, size);
    }
    media_lib_mutex_lock(mem_trace->mutex, MEDIA_LIB_MAX_LOCK_TIME);
    void *ptr = mem_trace->kept.realloc(buf, size);
    if (buf) {
        remove_trace(buf);
    }
    if (ptr) {
        add_trace(module, ptr, size, 0);
    }
    media_lib_mutex_unlock(mem_trace->mutex);
    return ptr;
}

char *media_lib_module_strdup(const char *module, const char *str)
{
    if (trace_cfg.trace_type == MEDIA_LIB_MEM_TRACE_NONE) {
        return media_lib_strdup(str);
    }
    char *ptr = mem_trace->kept.strdup(str);
    if (ptr) {
        int len = strlen(ptr) + 1;
        add_trace(module, ptr, len, 0);
    }
    return ptr;
}

int media_lib_start_mem_trace(media_lib_mem_trace_cfg_t *cfg)
{
    if (cfg == NULL || cfg->trace_type == MEDIA_LIB_MEM_TRACE_NONE) {
        return ESP_MEDIA_ERR_INVALID_ARG;
    }
    if (trace_cfg.trace_type != MEDIA_LIB_MEM_TRACE_NONE || mem_trace) {
        ESP_LOGI(TAG, "Already started");
        return ESP_MEDIA_ERR_OK;
    }
    esp_media_err_t ret = ESP_MEDIA_ERR_OK;
    media_lib_mem_t mem_lib = {};
    media_lib_get_mem_lib(&mem_lib);
    if (mem_lib.calloc == NULL) {
        ESP_LOGE(TAG, "Memory library not install yet");
        return ESP_MEDIA_ERR_INVALID_ARG;
    }
    mem_trace = (mem_trace_t *) mem_lib.calloc(1, sizeof(mem_trace_t));
    if (mem_trace == NULL) {
        return ESP_MEDIA_ERR_NO_MEM;
    }
    do {
        memcpy(&mem_trace->kept, &mem_lib, sizeof(mem_lib));
        if (media_lib_mutex_create(&mem_trace->mutex) != ESP_MEDIA_ERR_OK) {
            ret = ESP_MEDIA_ERR_NO_MEM;
            break;
        }
        int n = cfg->record_num;
        if (cfg->trace_type & (MEDIA_LIB_MEM_TRACE_MODULE_USAGE | MEDIA_LIB_MEM_TRACE_LEAK)) {
            if (n == 0) {
                n = MEDIA_LIB_DEFAULT_TRACE_NUM;
            }
        }
        if (n) {
            mem_trace->item_size = sizeof(mem_trace_item_t) + cfg->stack_depth * sizeof(void *);
            mem_trace->trace_item = (mem_trace_item_t *) mem_trace->kept.calloc(1, mem_trace->item_size * n);
            if (mem_trace->trace_item == NULL) {
                ret = ESP_MEDIA_ERR_NO_MEM;
                break;
            }
        }
        if (cfg->trace_type & MEDIA_LIB_MEM_TRACE_SAVE_HISTORY) {
            ret = media_lib_start_mem_his(cfg);
            if (ret != ESP_MEDIA_ERR_OK) {
                ESP_LOGE(TAG, "Fail to preparing for save history");
                break;
            }
        }
        trace_cfg = *cfg;
        if (trace_cfg.stack_depth >= MAX_STACK_DEPTH) {
            trace_cfg.stack_depth = MAX_STACK_DEPTH;
        }
        trace_cfg.record_num = n;
        mem_lib.malloc = _malloc;
        mem_lib.free = _free;
        mem_lib.malloc_align = _malloc_align,
        mem_lib.free_align = _free_align,
        mem_lib.calloc = _calloc;
        mem_lib.realloc = _realloc;
        mem_lib.strdup = _strdup;
        media_lib_set_mem_lib(&mem_lib);
        ESP_LOGI(TAG, "Start memory trace OK   eeeeee");
        return ESP_MEDIA_ERR_OK;
    } while (0);
    media_lib_stop_mem_trace();
    return ret;
}

void media_lib_stop_mem_trace(void)
{
    if (mem_trace == NULL) {
        return;
    }
    media_lib_mem_trace_type_t trace_type = trace_cfg.trace_type;
    if (trace_type) {
        trace_cfg.trace_type = MEDIA_LIB_MEM_TRACE_NONE;
        media_lib_set_mem_lib(&mem_trace->kept);
    }
    print_mem_usage(NULL);
    if (trace_type & MEDIA_LIB_MEM_TRACE_SAVE_HISTORY) {
        media_lib_stop_mem_his();
    }
    if (trace_type & MEDIA_LIB_MEM_TRACE_LEAK) {
        print_leak(NULL);
    }
    if (mem_trace->mutex) {
        media_lib_mutex_lock(mem_trace->mutex, MEDIA_LIB_MAX_LOCK_TIME);
        media_lib_mutex_unlock(mem_trace->mutex);
        media_lib_mutex_destroy(mem_trace->mutex);
        mem_trace->mutex = NULL;
    }
    if (mem_trace->module_lists) {
        free_module();
        mem_trace->module_lists = NULL;
    }
    if (mem_trace->trace_item) {
        mem_trace->kept.free(mem_trace->trace_item);
        mem_trace->trace_item = NULL;
    }
    mem_trace->kept.free(mem_trace);
    mem_trace = NULL;
}

int media_lib_get_mem_usage(const char *module, uint32_t *size, uint32_t *peak_size)
{
    if (trace_cfg.trace_type == MEDIA_LIB_MEM_TRACE_NONE) {
        return ESP_MEDIA_ERR_WRONG_STATE;
    }
    int ret = ESP_MEDIA_ERR_OK;
    media_lib_mutex_lock(mem_trace->mutex, MEDIA_LIB_MAX_LOCK_TIME);
    if (module) {
        module_mem_info_t *m = get_module(module);
        if (m) {
            if (size) {
                *size = m->mem_usage;
            }
            if (peak_size) {
                *peak_size = m->peak_mem_usage;
            }
        } else {
            ret = ESP_MEDIA_ERR_NOT_FOUND;
        }
    } else {
        if (size) {
            *size = mem_trace->mem_usage;
        }
        if (peak_size) {
            *peak_size = mem_trace->peak_mem_usage;
        }
    }
    media_lib_mutex_unlock(mem_trace->mutex);
    return ret;
}

int media_lib_print_leakage(const char *module)
{
    if (trace_cfg.trace_type == MEDIA_LIB_MEM_TRACE_NONE) {
        return ESP_MEDIA_ERR_WRONG_STATE;
    }
    media_lib_mutex_lock(mem_trace->mutex, MEDIA_LIB_MAX_LOCK_TIME);
    int leak_size = print_leak(module);
    media_lib_mutex_unlock(mem_trace->mutex);
    return leak_size;
}

int media_lib_add_trace_mem(const char *module, void *ptr, int size, uint8_t flag)
{
    if (trace_cfg.trace_type == MEDIA_LIB_MEM_TRACE_NONE) {
        return ESP_MEDIA_ERR_WRONG_STATE;
    }
    if (ptr) {
        add_trace(module, ptr, size, flag);
    }
    return ESP_MEDIA_ERR_OK;
}

void media_lib_remove_trace_mem(void *ptr)
{
    if (trace_cfg.trace_type == MEDIA_LIB_MEM_TRACE_NONE) {
        return;
    }
    remove_trace(ptr);
}
