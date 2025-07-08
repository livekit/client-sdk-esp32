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
#ifndef MEDIA_LIB_MEM_HIS_H
#define MEDIA_LIB_MEM_HIS_H

#include "media_lib_mem_trace.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief      Start memory history trace
 * @param       cfg: Memory trace configuration
 * @return       - ESP_MEDIA_ERR_NO_MEM: Not enough memory
 *               - ESP_MEDIA_ERR_FAIL: Not resource
 *               - ESP_MEDIA_ERR_OK: On success
 */
int media_lib_start_mem_his(media_lib_mem_trace_cfg_t *cfg);

/**
 * @brief      Add malloc action to history
 * @param       addr: Memory address
 * @param       size: Memory size
 * @param       stack_num: Call stack number to record
 * @param       stack: Stack frames to record
 * @param       flag: Flag to record
 */
void media_lib_add_mem_malloc_his(void *addr, int size, int stack_num, void *stack, uint8_t flag);

/**
 * @brief      Add free action to history
 * @param       addr: Memory address
 */
void media_lib_add_mem_free_his(void *addr);

/**
 * @brief      Stop memory history trace
 */
void media_lib_stop_mem_his(void);

#ifdef __cplusplus
}
#endif

#endif
