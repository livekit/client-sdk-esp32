/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2023 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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
#ifndef MEDIA_LIB_MEM_TRACE_H
#define MEDIA_LIB_MEM_TRACE_H

#include "media_lib_os.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MEDIA_LIB_DEFAULT_TRACE_NUM       (1024)
#define MEDIA_LIB_DEFAULT_SAVE_CACHE_SIZE (64 * 1024)
#define MEDIA_LIB_DEFAULT_SAVE_PATH       "/sdcard/T.log"

/**
 * @brief      Memory trace type
 */
typedef enum {
    MEDIA_LIB_MEM_TRACE_NONE,                    /*!< No memory trace */
    MEDIA_LIB_MEM_TRACE_MODULE_USAGE = (1 << 0), /*!< Trace for module memory usage */
    MEDIA_LIB_MEM_TRACE_LEAK = (1 << 1),         /*!< Trace for memory leakage */
    MEDIA_LIB_MEM_TRACE_SAVE_HISTORY = (1 << 2), /*!< Save memory history to file for offline analysis */
    MEDIA_LIB_MEM_TRACE_ALL = (MEDIA_LIB_MEM_TRACE_MODULE_USAGE |
                               MEDIA_LIB_MEM_TRACE_LEAK | 
                               MEDIA_LIB_MEM_TRACE_SAVE_HISTORY),
} media_lib_mem_trace_type_t;

/**
 * @brief      Memory trace configuration
 */
typedef struct {
    media_lib_mem_trace_type_t trace_type;      /*!< Memory tracing type */
    uint8_t                    stack_depth;     /*!< Max stack depth to trace for malloc */
    int                        record_num;      /*!< Default is MEDIA_LIB_DEFAULT_TRACE_NUM if not provided */
    int                        save_cache_size; /*!< Default is MEDIA_LIB_DEFAULT_SAVE_CACHE_SIZE if not provided,
                                                    if malloc frequently to avoid overflow need enlarge this value */
    const char                *save_path;
} media_lib_mem_trace_cfg_t;

/**
 * @brief      Memory library function pointers
 */
typedef struct {
    __media_lib_os_malloc        malloc;          /*!< Malloc wrapper */
    __media_lib_os_free          free;            /*!< Free wrapper */
    __media_lib_os_malloc_align  malloc_align;    /*!< Malloc align wrapper */
    __media_lib_os_free_align    free_align;      /*!< Malloc align wrapper */
    __media_lib_os_calloc        calloc;          /*!< Calloc wrapper */
    __media_lib_os_realloc       realloc;         /*!< Realloc wrapper */
    __media_lib_os_strdup        strdup;          /*!< Strdup wrapper */
    __media_lib_os_stack_frame   get_stack_frame; /*!< Get stack frame wrapper */
} media_lib_mem_t;

/**
 * @brief      Get memory library function group
 * @param[out]   mem_lib: Memory library to store
 * @return       - ESP_MEDIA_ERR_INVALID_ARG: Invalid input argument
 *               - ESP_MEDIA_ERR_OK: On success
 */
int media_lib_get_mem_lib(media_lib_mem_t *mem_lib);

/**
 * @brief      Set memory library function group
 *             When memory trace started, this API is called internally to replace default memory libary
 *             After stop default library is restored
 * @param[in]   mem_lib: New memory libary to set
 * @return       - ESP_MEDIA_ERR_INVALID_ARG: Invalid input argument
 *               - ESP_MEDIA_ERR_OK: On success
 */
int media_lib_set_mem_lib(media_lib_mem_t *mem_lib);

/**
 * @brief      Start memory trace
 * @param       cfg: Memory trace configuration
 * @return       - ESP_MEDIA_ERR_INVALID_ARG: Invalid input argument
 *               - ESP_MEDIA_ERR_OK: On success
 */
int media_lib_start_mem_trace(media_lib_mem_trace_cfg_t *cfg);

/**
 * @brief      Add memory to trace
 * @param       module: Module to be traced (can set to NULL)
 * @param       addr: Traced memory address
 * @param       size: Traced memory size
 * @param       flag: Customized trace flag
 * @return       - ESP_MEDIA_ERR_WRONG_STATE: Memory trace not started yet
 *               - ESP_MEDIA_ERR_OK: On success
 */
int media_lib_add_trace_mem(const char *module, void *addr, int size, uint8_t flag);

/**
 * @brief      Remove trace memory
 * @param       addr: Traced memory address
 */
void media_lib_remove_trace_mem(void *addr);

/**
 * @brief      Get memory usage
 * @param       module:  Module to be traced (set NULL to get memory usage of all modules)
 * @param[out]  used_size: Total memory currently used by module
 * @param[out]  peak_size: Peak memory size used by module
 * @return       - ESP_MEDIA_ERR_WRONG_STATE: Memory trace not started yet
 *               - ESP_MEDIA_ERR_NOT_FOUND: Module not found
 *               - ESP_MEDIA_ERR_OK: On success
 */
int media_lib_get_mem_usage(const char *module, uint32_t *used_size, uint32_t *peak_size);

/**
 * @brief      Print memory leakage
 *             Notes: When use `idf.py monitor` the leakage address will automatically convert to function line
 *                    If use other tools, please use `addr2line` to convert the address to function line
 * @param       module:  Module to be traced (set NULL to print memory leakage of all modules)
 * @return       - ESP_MEDIA_ERR_WRONG_STATE: Memory trace not started yet
 *               - Others: Leakage memory size
 */
int media_lib_print_leakage(const char *module);

/**
 * @brief      Stop memory trace
 */
void media_lib_stop_mem_trace(void);

#ifdef __cplusplus
}
#endif

#endif
