/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2021 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#ifndef MEDIA_LIB_OS_H
#define MEDIA_LIB_OS_H

#include <string.h>
#include "media_lib_os_reg.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MEDIA_LIB_MAX_LOCK_TIME (0xFFFFFFFF)

/**
 * @brief      Configuration for thread schedule
 */
typedef struct {
    uint8_t  priority;    /*!< Thread priority */
    uint8_t  core_id;     /*!< CPU core id for thread to run */
    uint32_t stack_size;  /*!< Thread reserve stack size */
} media_lib_thread_cfg_t;

/**
 * @brief      Callback to get thread schedule parameter
 */
typedef void (*media_lib_thread_sched_param_cb)(const char *thread_name, media_lib_thread_cfg_t *thread_cfg);

/**
 * @brief      Helper to set scheduler in callback (using default callback signature)
 */
#define MEDIA_LIB_THREAD_SCHEDULE_SET(_thread, _stack_size, _priority, _core_id) \
if (strcmp(thread_name, _thread) == 0) {                                         \
    thread_cfg->stack_size = _stack_size;                                        \
    thread_cfg->priority = _priority;                                            \
    thread_cfg->core_id = _core_id;                                              \
}

/**
 * @brief      Wrapper for malloc
 */
void *media_lib_malloc(size_t size);

/**
 * @brief      Wrapper for free
 */
void media_lib_free(void *buf);

/**
 * @brief      Wrapper for malloc alignment
 */
void *media_lib_malloc_align(size_t size, uint8_t align);

/**
 * @brief      Wrapper for free alignment
 */
void media_lib_free_align(void *buf);

/**
 * @brief      Wrapper for calloc
 */
void *media_lib_calloc(size_t num, size_t size);

/**
 * @brief      Wrapper for realloc
 */
void *media_lib_realloc(void *buf, size_t size);

/**
 * @brief      Wrapper for strdup
 */
char *media_lib_strdup(const char *str);

/**
 * @brief      asprintf wrapper
 */
int media_lib_asprintf(char **str, const char *fmt, ...);

/**
 * @brief      Module malloc
 */
void *media_lib_module_malloc(const char* module, size_t size);

/**
 * @brief      Module calloc
 */
void *media_lib_module_calloc(const char* module, size_t num, size_t size);

/**
 * @brief      Module realloc
 */
void *media_lib_module_realloc(const char* module, void *buf, size_t size);

/**
 * @brief      Module strdup
 */
char *media_lib_module_strdup(const char* module, const char *str);

/**
 * @brief      Get stack frame
 * @param         addr: Address array to store stack frame PC
 * @param         n: Stack depth to trace
 * @return        Stack depth be traced
 */
int media_lib_get_stack_frame(void** addr, int n);

/**
 * @brief      Wrapper for thread create
 * @param[out]    handle: Thread handle
 * @param         name: Thread name
 * @param         body: Thread body
 * @param         arg:  Thread argument
 * @param         stack_size: Thread stacksize set
 * @param         prio: Thread priority
 * @param         core: Run on which cpu core
 * @return        - ESP_OK: On success
 *                - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *                - Others: thread create fail
 */
int media_lib_thread_create(media_lib_thread_handle_t *handle, const char *name, void(*body)(void *arg), void *arg,
                                   uint32_t stack_size, int prio, int core);

/**
 * @brief      Set thread schedule callback
 * @param         cb  Callback to get thread schedule setting
 */
void media_lib_thread_set_schedule_cb(media_lib_thread_sched_param_cb cb);

/**
 * @brief      Create thread using schedule callback
 *             NOTES: When callback is not set or not overwrote, it will use default setting
                      Default stack size is 4K, priority is 10, run on core 0
 * @param[out]    handle: Thread handle
 * @param         name: Thread name
 * @param         body: Thread body
 * @param         arg: Thread argument
 * @return        - ESP_OK: On success
 *                - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *                - Others: thread create fail
 */
int media_lib_thread_create_from_scheduler(media_lib_thread_handle_t *handle, const char *name, void(*body)(void *arg), void *arg);

/**
 * @brief      Wrapper for thread destroy
 * @param         handle: Thread handle
 */
void media_lib_thread_destroy(media_lib_thread_handle_t handle);

/**
 * @brief      Wrapper for thread set priority
 * @param        handle: Thread handle
 * @param        prio: Thread priority
 * @return       - true:  On success
 *               - false: Set priority fail
 */
bool media_lib_thread_set_priority(media_lib_thread_handle_t handle, int prio);

/**
 * @brief      Wrapper for thread sleep
 * @param         ms: Sleep time millisecond
 */
void media_lib_thread_sleep(uint32_t ms);

/**
 * @brief      Wrapper for sema create
 * @param[out]   sema: Semaphore handle
 * @return       - 0: On success
 *               - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *               - Others: Sema create fail
 */
int media_lib_sema_create(media_lib_sema_handle_t *sema);

/**
 * @brief      Wrapper for sema lock with timeout
 * @param        sema: Semaphore handle
 * @param        timeout: Lock timeout in milliseconds
 * @return       - 0: On success
 *               - ESP_ERR_NOT_SUPPORTED: Wrapper function not registered
 *               - Others: Sema lock fail
 */
int media_lib_sema_lock(media_lib_sema_handle_t sema, uint32_t timeout);

/**
 * @brief      Wrapper for sema unlock
 * @param        sema: Semaphore handle
 * @return       - 0: On success
 *               - ESP_ERR_NOT_SUPPORTED: Wrapper function not registered
 *               - Others: Sema unlock fail
 */
int media_lib_sema_unlock(media_lib_sema_handle_t sema);

/**
 * @brief      Wrapper for sema destroy
 * @param        sema: Semaphore handle
 * @return       - 0: On success
 *               - ESP_ERR_NOT_SUPPORTED: Wrapper function not registered
 *               - Others: Mutex destroy fail
 */
int media_lib_sema_destroy(media_lib_sema_handle_t sema);

/**
 * @brief      Wrapper for mutex create
 * @param[out]   mutex: Mutex handle
 * @return       - 0: On success
 *               - ESP_ERR_NOT_SUPPORTED: Wrapper function not registered
 *               - Others: Mutex create fail
 */
int media_lib_mutex_create(media_lib_mutex_handle_t *mutex);

/**
 * @brief      Wrapper for mutex lock with timeout
 * @param        mutex: Mutex handle
 * @param        timeout: Lock timeout in milliseconds
 * @return       - 0: On success
 *               - ESP_ERR_NOT_SUPPORTED: Wrapper function not registered
 *               - Others: Mutex lock fail
 */
int media_lib_mutex_lock(media_lib_mutex_handle_t mutex, uint32_t timeout);

/**
 * @brief      Wrapper for mutex unlock
 * @param        mutex: Mutex handle
 * @return       - 0: On success
 *               - ESP_ERR_NOT_SUPPORTED: Wrapper function not registered
 *               - Others: Mutex unlock fail
 */
int media_lib_mutex_unlock(media_lib_mutex_handle_t mutex);

/**
 * @brief      Wrapper for mutex destroy
 * @param        mutex: Mutex handle
 * @return       - 0: On success
 *               - ESP_ERR_NOT_SUPPORTED: Wrapper function not registered
 *               - Others: Mutex destroy fail
 */
int media_lib_mutex_destroy(media_lib_mutex_handle_t mutex);

/**
 * @brief      Wrapper for enter critical section
 * @return       - 0: On success
 *               - ESP_ERR_NOT_SUPPORTED: Wrapper function not registered
 *               - Others: Enter critical section fail
 */
int media_lib_enter_critical_section(void);

/**
 * @brief      Wrapper for leave critical section
 * @return       - 0: On success
 *               - ESP_ERR_NOT_SUPPORTED: Wrapper function not registered
 *               - Others: Leave critical section fail
 */
int media_lib_leave_critical_section(void);

/**
 * @brief      Wrapper for event group create
 * @param[out]   event_group: Event group handle pointer
 * @return       - 0: On success
 *               - ESP_ERR_NOT_SUPPORTED: Wrapper function not registered
 *               - Others: Event group create fail
 */
int media_lib_event_group_create(media_lib_event_grp_handle_t *event_group);

/**
 * @brief      Wrapper for event group set bits
 * @param        event_group: Event group handle
 * @param        bits: Set bits mask
 * @return       Bits being set currently
 */
uint32_t media_lib_event_group_set_bits(media_lib_event_grp_handle_t event_group, uint32_t bits);

/**
 * @brief      Wrapper for event group clear bits
 * @param        event_group: Event group handle
 * @param        bits: Clear bits mask
 * @return       Bits being set currently
 */
uint32_t media_lib_event_group_clr_bits(media_lib_event_grp_handle_t event_group, uint32_t bits);

/**
 * @brief      Wrapper for event group wait bits
 * @param        event_group: Event group handle
 * @param        bits: Waiting for bits which will be set
 * @param        timeout: Wait timeout in milliseconds
 * @return       Bits being set currently
 */
uint32_t media_lib_event_group_wait_bits(media_lib_event_grp_handle_t event_group, uint32_t bits, uint32_t timeout);

/**
 * @brief      Wrapper for event group destroy
 * @param        event_group: Event group handle
 * @return       - 0: On success
 *               - ESP_ERR_NOT_SUPPORTED: Wrapper function not registered
 *               - Others: Event group destroy fail
 */
int media_lib_event_group_destroy(media_lib_event_grp_handle_t event_group);

#ifdef __cplusplus
}
#endif

#endif
