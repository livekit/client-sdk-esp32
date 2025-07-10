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

#ifndef MEDIA_LIB_OS_REG_H
#define MEDIA_LIB_OS_REG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* (*__media_lib_os_malloc)(size_t size);
typedef void (*__media_lib_os_free)(void* buf);
typedef void* (*__media_lib_os_calloc)(size_t num, size_t size);
typedef void* (*__media_lib_os_realloc)(void* buf, size_t size);
typedef char* (*__media_lib_os_strdup)(const char* str);
typedef void* (*__media_lib_os_malloc_align)(size_t size, uint8_t align);
typedef void (*__media_lib_os_free_align)(void* buf);
typedef int (*__media_lib_os_stack_frame)(void** addr, int n);

typedef void *media_lib_thread_handle_t;
typedef int (*__media_lib_os_thread_create)(media_lib_thread_handle_t *handle, const char *name, void(*body)(void *arg), void *arg,
                                           uint32_t stack_size, int prio, int core);
typedef void (*__media_lib_os_thread_destroy)(media_lib_thread_handle_t handle);
typedef bool (*__media_lib_os_thread_set_priority)(media_lib_thread_handle_t handle, int prio);
typedef void (*__media_lib_os_thread_sleep)(uint32_t ms);

typedef void *media_lib_sema_handle_t;
typedef int (*__media_lib_os_sema_create)(media_lib_sema_handle_t *sema);
typedef int (*__media_lib_os_sema_lock)(media_lib_sema_handle_t sema, uint32_t timeout);
typedef int (*__media_lib_os_sema_unlock)(media_lib_sema_handle_t sema);
typedef int (*__media_lib_os_sema_destroy)(media_lib_sema_handle_t sema);

typedef void *media_lib_mutex_handle_t;
typedef int (*__media_lib_os_mutex_create)(media_lib_mutex_handle_t *mutex);
typedef int (*__media_lib_os_mutex_lock)(media_lib_mutex_handle_t mutex, uint32_t timeout);
typedef int (*__media_lib_os_mutex_unlock)(media_lib_mutex_handle_t mutex);
typedef int (*__media_lib_os_mutex_destroy)(media_lib_mutex_handle_t mutex);

typedef int (*__media_lib_os_enter_critical_section)();
typedef int (*__media_lib_os_leave_critical_section)();

typedef void *media_lib_event_grp_handle_t;
typedef int (*__media_lib_os_event_group_create)(media_lib_event_grp_handle_t *event_group);
typedef uint32_t (*__media_lib_os_event_group_set_bits)(media_lib_event_grp_handle_t event_group, uint32_t bits);
typedef uint32_t (*__media_lib_os_event_group_clr_bits)(media_lib_event_grp_handle_t event_group, uint32_t bits);
typedef uint32_t (*__media_lib_os_event_group_wait_bits)(media_lib_event_grp_handle_t event_group, uint32_t bits, uint32_t timeout);
typedef int (*__media_lib_os_event_group_destroy)(media_lib_event_grp_handle_t event_group);

/**
 * @brief      struct for OS related wrapper functions
 */
typedef struct {
    __media_lib_os_malloc                  malloc;              /*!< malloc wrapper */
    __media_lib_os_free                    free;                /*!< free wrapper */
    __media_lib_os_calloc                  calloc;              /*!< calloc wrapper */
    __media_lib_os_realloc                 realloc;             /*!< realloc wrapper */
    __media_lib_os_strdup                  strdup;              /*!< strdup wrapper */
    __media_lib_os_malloc_align            malloc_align;        /*!< malloc align */
    __media_lib_os_free_align              free_align;          /*!< free align */
    __media_lib_os_stack_frame             get_stack_frame;     /*!< stack frame */

    __media_lib_os_thread_create           thread_create;       /*!< thread create wrapper */
    __media_lib_os_thread_destroy          thread_destroy;      /*!< thread destroy wrapper */
    __media_lib_os_thread_set_priority     thread_set_prio;     /*!< set thread priority wrapper */
    __media_lib_os_thread_sleep            thread_sleep;        /*!< thread sleep wrapper */

    __media_lib_os_sema_create             sema_create;         /*!< sema create wrapper */
    __media_lib_os_sema_lock               sema_lock;           /*!< sema lock wrapper */
    __media_lib_os_sema_unlock             sema_unlock;         /*!< sema unlock wrapper */
    __media_lib_os_sema_destroy            sema_destroy;        /*!< sema destroy wrapper */

    __media_lib_os_mutex_create            mutex_create;        /*!< mutex create wrapper */
    __media_lib_os_mutex_lock              mutex_lock;          /*!< mutex lock wrapper */
    __media_lib_os_mutex_unlock            mutex_unlock;        /*!< mutex unlock wrapper */
    __media_lib_os_mutex_destroy           mutex_destroy;       /*!< mutex destroy wrapper */

    __media_lib_os_enter_critical_section  enter_critical;      /*!< enter critical wrapper */
    __media_lib_os_leave_critical_section  leave_critical;      /*!< leave critical wrapper */

    __media_lib_os_event_group_create      group_create;        /*!< event group create wrapper */
    __media_lib_os_event_group_set_bits    group_set_bits;      /*!< event group set bits wrapper */
    __media_lib_os_event_group_clr_bits    group_clr_bits;      /*!< event group clear bits  wrapper */
    __media_lib_os_event_group_wait_bits   group_wait_bits;     /*!< event group wait for bits wrapper */
    __media_lib_os_event_group_destroy     group_destroy;       /*!< event group destroy wrapper */
} media_lib_os_t;

/**
 * @brief     Register OS related wrapper functions for media library
 *
 * @param      os_lib  OS wrapper function lists
 *
* @return
*             - ESP_OK: on success
*             - ESP_ERR_INVALID_ARG: some members of OS lib not set
*/
esp_err_t media_lib_os_register(media_lib_os_t *os_lib);

#ifdef __cplusplus
}
#endif

#endif


