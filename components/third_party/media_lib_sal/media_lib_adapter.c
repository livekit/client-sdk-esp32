/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2021 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#include "esp_log.h"
#include "media_lib_adapter.h"
#include "media_lib_mem_trace.h"

#define TAG "MEDIA_ADAPTER"

#ifdef CONFIG_MEDIA_LIB_MEM_AUTO_TRACE
static void add_memory_trace(void)
{
    media_lib_mem_trace_cfg_t trace_cfg = {0};
#ifdef CONFIG_MEDIA_LIB_MEM_TRACE_MODULE
    trace_cfg.trace_type |= MEDIA_LIB_MEM_TRACE_MODULE_USAGE;
#endif
#ifdef CONFIG_MEDIA_LIB_MEM_TRACE_LEAKAGE
    trace_cfg.trace_type |= MEDIA_LIB_MEM_TRACE_LEAK;
#endif
#ifdef CONFIG_MEDIA_LIB_MEM_TRACE_SAVE_HISTORY
    trace_cfg.trace_type |= MEDIA_LIB_MEM_TRACE_SAVE_HISTORY;
    trace_cfg.save_cache_size = CONFIG_MEDIA_LIB_MEM_SAVE_CACHE_SIZE;
    trace_cfg.save_path = CONFIG_MEDIA_LIB_MEM_TRACE_SAVE_PATH;
#endif
    trace_cfg.stack_depth = CONFIG_MEDIA_LIB_MEM_TRACE_DEPTH;
    trace_cfg.record_num = CONFIG_MEDIA_LIB_MEM_TRACE_NUM;
    media_lib_start_mem_trace(&trace_cfg);
}
#endif

esp_err_t media_lib_add_default_adapter(void)
{
    esp_err_t ret;
    ret = media_lib_add_default_os_adapter();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fail to add os lib");
    }
#ifdef CONFIG_MEDIA_PROTOCOL_LIB_ENABLE
    ret = media_lib_add_default_crypt_adapter();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fail to add crypt lib");
    }
    ret = media_lib_add_default_socket_adapter();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fail to add socket lib");
    }
    ret = media_lib_add_default_tls_adapter();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fail to add tls lib");
    }
    ret = media_lib_add_default_netif_adapter();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fail to add netif lib");
    }
#endif
#ifdef CONFIG_MEDIA_LIB_MEM_AUTO_TRACE
    add_memory_trace();
#endif
    return ret;
}
