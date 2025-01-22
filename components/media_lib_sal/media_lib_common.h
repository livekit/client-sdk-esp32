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

#ifndef MEDIA_LIB_COMMON_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <string.h>
#include "esp_err.h"

/**
 * @brief     verify library functions pointer all set or not
 *
 * @param     lib  wrapper function struct pointer
 * @param     size wrapper struct size
 * @return
 *             -true  library verify OK
 *             -false library verify Fail
 */
bool media_lib_verify(void *lib, int size);

#define MEDIA_LIB_DEFAULT_INSTALLER(src, dst, type)                            \
    if (media_lib_verify(src, sizeof(type)) == false) {                        \
        return ESP_ERR_INVALID_ARG;                                            \
    }                                                                          \
    memcpy(dst, src, sizeof(type));                                            \
    return ESP_OK;

#ifdef __cplusplus
}
#endif

#endif