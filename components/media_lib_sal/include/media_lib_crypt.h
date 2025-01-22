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

#ifndef MEDIA_LIB_CRYPT_H
#define MEDIA_LIB_CRYPT_H

#include "media_lib_crypt_reg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief     MD5 initialize wrapper
 *
 * @param[out]  ctx: MD5 struct pointer
 */
void media_lib_md5_init(media_lib_md5_handle_t *ctx);

/**
 * @brief     MD5 resource free wrapper
 *
 * @param     ctx: MD5 instance
 */
void media_lib_md5_free(media_lib_md5_handle_t ctx);

/**
 * @brief     MD5 start wrapper
 *
 * @param     ctx: MD5 instance
 * 
 * @return      
 *              - 0: On success
 *              - ESP_ERR_NOT_SUPPORTED: Wrapper function not registered
 *              - Others: MD5 init fail
 */
int media_lib_md5_start(media_lib_md5_handle_t ctx);

/**
 * @brief      MD5 add input data
 * 
 * @param      ctx: MD5 instance
 * @param      input: Input data
 * @param      len: Input data length
 * 
 * @return      
 *              - 0: On success
 *              - ESP_ERR_NOT_SUPPORTED: Wrapper function not registered
 *              - Others: MD5 update fail
 */
int media_lib_md5_update(media_lib_md5_handle_t ctx, const unsigned char *input, size_t len);

/**
 * @brief      Get MD5 output
 * 
 * @param      ctx: MD5 instance
 * @param      output: MD5 output
 * 
 * @return      
 *              - 0: On success
 *              - ESP_ERR_NOT_SUPPORTED: Wrapper function not registered
 *              - Others: md5 finish fail
 */
int media_lib_md5_finish(media_lib_md5_handle_t ctx, unsigned char output[16]);

/**
 * @brief     SHA256 initialize wrapper
 *
 * @param[out]  ctx: SHA256 instance pointer
 */
void media_lib_sha256_init(media_lib_sha256_handle_t *ctx);

/**
 * @brief     SHA256 resource free wrapper
 *
 * @param     ctx: SHA256 instance
 */
void media_lib_sha256_free(media_lib_sha256_handle_t ctx);

/**
 * @brief     SHA256 start wrapper
 *
 * @param     ctx: SHA256 instance
 * 
 * @return      
 *              - 0: On success
 *              - ESP_ERR_NOT_SUPPORTED: Wrapper function not registered
 *              - Others: SHA256 start fail
 */
int media_lib_sha256_start(media_lib_sha256_handle_t ctx);

/**
 * @brief      SHA256 add input data
 * 
 * @param      ctx: SHA256 instance
 * @param      input: Input data
 * @param      len: Input data length
 * 
 * @return      
 *              - 0: On success
 *              - ESP_ERR_NOT_SUPPORTED: Wrapper function not registered
 *              - Others: SHA256 update fail
 */
int media_lib_sha256_update(media_lib_sha256_handle_t ctx, const unsigned char *input, size_t len);

/**
 * @brief      Get SHA256 output
 * 
 * @param      ctx     SHA256 instance
 * @param      output  SHA256 value of input
 * 
 * @return      
 *              - 0: On success
 *              - ESP_ERR_NOT_SUPPORTED: Wrapper function not registered
 *              - Others: SHA256 finish fail
 */
int media_lib_sha256_finish(media_lib_sha256_handle_t ctx, unsigned char output[32]);

/**
 * @brief      AES initialize wrapper
 *
 * @param[out]  ctx: AES instance pointer
 */
void media_lib_aes_init(media_lib_aes_handle_t *ctx);

/**
 * @brief     AES resource free wrapper
 *
 * @param     ctx: AES instance
 */
void media_lib_aes_free(media_lib_aes_handle_t ctx);

/**
 * @brief     AES set key
 *
 * @param     ctx: AES instance
 * @param     key: AES key
 * @param     key_bits: Bitlength of key
 * 
 *  @return
 *              - 0: On success
 *              - ESP_ERR_NOT_SUPPORTED: Wrapper function not registered
 *              - Others: AES set key fail
 * 
 */
int media_lib_aes_set_key(media_lib_aes_handle_t ctx, uint8_t *key, uint8_t key_bits);

/**
 * @brief     AES-CBC encryption/decryption
 *
 * @note      
 * @param     ctx: AES instance
 * @param     decrypt_mode: Set `true` for decryption, `false` for encryption
 * @param     iv: AES iv information (iv will be updated after each call it must be writable)
 * @param     input: Input data to decrypt/encrypt
 * @param     size: Data size
 * @param     output: Output data
 * 
 *  @return      
 *              - 0: On success
 *              - ESP_ERR_NOT_SUPPORTED: Wrapper function not registered
 *              - Others: Encrypt/decrypt fail
 * 
 */
int media_lib_aes_crypt_cbc(media_lib_aes_handle_t ctx, bool decrypt_mode, uint8_t iv[16], uint8_t *input, size_t size, uint8_t *output);

#ifdef __cplusplus
}
#endif

#endif