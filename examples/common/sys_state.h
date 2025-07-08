/* System state

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Show current system memory usage and cpu usage
 */
void sys_state_show(void);

/**
 * @brief  Trace for heap memory
 * 
 * @param[in]  start  Start or stop trace
 */
void sys_state_heap_trace(bool start);

#ifdef __cplusplus
}
#endif