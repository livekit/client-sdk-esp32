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
#ifndef MEDIA_LIB_CRYPT_REG_H
#define MEDIA_LIB_CRYPT_REG_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* media_lib_md5_handle_t;
typedef void (*__media_lib_md5_init)(media_lib_md5_handle_t *ctx);
typedef void (*__media_lib_md5_free)(media_lib_md5_handle_t ctx);
typedef int (*__media_lib_md5_start)(media_lib_md5_handle_t ctx);
typedef int (*__media_lib_md5_update_ret)(media_lib_md5_handle_t ctx, const unsigned char *input, size_t len);
typedef int (*__media_lib_md5_finish_ret)(media_lib_md5_handle_t ctx, unsigned char output[16]);

typedef void* media_lib_sha256_handle_t;
typedef void (*__media_lib_sha256_init)(media_lib_sha256_handle_t *ctx);
typedef void (*__media_lib_sha256_free)(media_lib_sha256_handle_t ctx);
typedef int (*__media_lib_sha256_start)(media_lib_sha256_handle_t ctx);
typedef int (*__media_lib_sha256_update_ret)(media_lib_sha256_handle_t ctx, const unsigned char *input, size_t len);
typedef int (*__media_lib_sha256_finish_ret)(media_lib_sha256_handle_t ctx, unsigned char output[32]);

typedef void* media_lib_aes_handle_t;
typedef void (*__media_lib_aes_init)(media_lib_aes_handle_t *ctx);
typedef void (*__media_lib_aes_free)(media_lib_aes_handle_t ctx);
typedef int (*__media_lib_aes_set_key)(media_lib_aes_handle_t ctx, uint8_t *key, uint8_t key_bits);
typedef int (*__media_lib_aes_crypt_cbc)(media_lib_aes_handle_t ctx, bool decrypt_mode, uint8_t iv[16],
                                        uint8_t *input, size_t size, uint8_t *output);
typedef struct {
    __media_lib_md5_init          md5_init;        /*!< MD5 lib init */
    __media_lib_md5_free          md5_free;        /*!< MD5 lib deinit */
    __media_lib_md5_start         md5_start;       /*!< MD5 start add data */
    __media_lib_md5_update_ret    md5_update;      /*!< MD5 start add extra data */
    __media_lib_md5_finish_ret    md5_finish;      /*!< get MD5 value */
    __media_lib_sha256_init       sha256_init;     /*!< SHA256 lib init */
    __media_lib_sha256_free       sha256_free;     /*!< SHA256 lib deinit */
    __media_lib_sha256_start      sha256_start;    /*!< SHA256 start add data */
    __media_lib_sha256_update_ret sha256_update;   /*!< SHA256 start add extra data */
    __media_lib_sha256_finish_ret sha256_finish;   /*!< Get SHA256 value */
    __media_lib_aes_init          aes_init;        /*!< AES lib init */
    __media_lib_aes_free          aes_free;        /*!< AES lib free */
    __media_lib_aes_set_key       aes_set_key;     /*!< AES set key */
    __media_lib_aes_crypt_cbc     aes_crypt_cbc;   /*!< AES-CBC encrypt/decrypt */
} media_lib_crypt_t;

/**
 * @brief     Register Crypt related wrapper functions for media library
 *
 * @param      crypt_lib  Crypt wrapper function lists
 *
* @return
*             - ESP_OK: on success
*             - ESP_ERR_INVALID_ARG: some members of crypt lib not set
*/
esp_err_t media_lib_crypt_register(media_lib_crypt_t *crypt_lib);

#ifdef __cplusplus
}
#endif

#endif
