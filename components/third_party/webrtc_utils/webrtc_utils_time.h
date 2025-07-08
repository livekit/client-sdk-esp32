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

#pragma once

#include <inttypes.h>
#include <esp_err.h>

/**
 * @brief Get current time in IO8601 format
 *
 * @return char* Time string
 * @note Caller is supposed to free the output string
 */
void webrtc_utils_get_time_iso8601(char time_buf[18]);

/**
 * @brief Wait for timeout_ms for time_sync to finish
 *
 * @param timeout_ms time in ms to wait for time_sync. Note, the actual time spent could be more
 * @return esp_err_t ESP_OK if success, apt error otherwise
 */
esp_err_t webrtc_utils_wait_for_time_sync(uint32_t timeout_ms);

/**
 * @brief Init sntp time_sync
 *
 * @return ESP_OK on success, apt error otherwise
 */
esp_err_t webrtc_utils_time_sync_init();
