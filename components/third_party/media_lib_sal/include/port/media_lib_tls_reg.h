/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2022 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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
#ifndef MEDIA_LIB_TLS_REG_H
#define MEDIA_LIB_TLS_REG_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *cacert_buf;               /*!< Certificate Authority's certificate in a buffer */
    int         cacert_bytes;             /*!< Size of Certificate Authority certificate */
    const char *clientcert_buf;           /*!< Client certificate legacy name */
    int         clientcert_bytes;         /*!< Size of client certificate legacy name */
    const char *clientkey_buf;            /*!< Client key legacy name */
    int         clientkey_bytes;          /*!< Size of client key legacy name */
    const char *clientkey_password;       /*!< Client key decryption password string */
    int         clientkey_password_len;   /*!< String length of the password */
    bool        non_block;                /*!< Configure non-blocking mode */
    bool        use_secure_element;       /*!< Enable this option to use secure element */
    int         timeout_ms;               /*!< Network timeout in milliseconds */
    bool        use_global_ca_store;      /*!< Use a global ca_store for all the connections */
    bool        skip_common_name;         /*!< Skip any validation of server certificate CN field */
    int (*crt_bundle_attach)(void *conf); /*!< Function pointer to esp_crt_bundle_attach */
} media_lib_tls_cfg_t;

typedef struct {
    const char *cacert_buf;             /*!< Client CA certificate in a buffer */
    int         cacert_bytes;           /*!< Size of client CA certificate */
    const char *servercert_buf;         /*!< Server certificate in a buffer */
    int         servercert_bytes;       /*!< Size of server certificate */
    const char *serverkey_buf;          /*!< Server key in a buffer */
    int         serverkey_bytes;        /*!< Size of server key */
    const char *serverkey_password;     /*!< Server key decryption password string */
    int         serverkey_password_len; /*!< String length of the password */
} media_lib_tls_server_cfg_t;

typedef void *media_lib_tls_handle_t;
typedef media_lib_tls_handle_t (*__media_lib_tls_new)(const char *hostname, int hostlen, int port,
                                                      const media_lib_tls_cfg_t *cfg);
typedef media_lib_tls_handle_t (*__media_lib_tls_new_server)(int sock_fd, const media_lib_tls_server_cfg_t *cfg);
typedef int (*__media_lib_tls_write)(media_lib_tls_handle_t tls, const void *data, size_t datalen);
typedef int (*__media_lib_tls_read)(media_lib_tls_handle_t tls, void *data, size_t datalen);
typedef int (*__media_lib_tls_getsockfd)(media_lib_tls_handle_t tls);
typedef int (*__media_lib_tls_delete)(media_lib_tls_handle_t tls);
typedef int (*__media_lib_tls_get_bytes_avail)(media_lib_tls_handle_t tls);

typedef struct {
    __media_lib_tls_new             tls_new;             /*!< tls lib new */
    __media_lib_tls_new_server      tls_new_server;      /*!< tls lib new server */
    __media_lib_tls_write           tls_write;           /*!< tls lib write */
    __media_lib_tls_read            tls_read;            /*!< tls lib read */
    __media_lib_tls_getsockfd       tls_getsockfd;       /*!< tls lib getsockfd */
    __media_lib_tls_delete          tls_delete;          /*!< tls lib delete */
    __media_lib_tls_get_bytes_avail tls_get_bytes_avail; /*!< tls lib get bytes avail */
} media_lib_tls_t;

/**
 * @brief     Register tls related wrapper functions for media library
 *
 * @param      tls_lib  tls wrapper function lists
 *
 * @return
 *             - ESP_OK: on success
 *             - ESP_ERR_INVALID_ARG: some members of tls lib not set
 */
esp_err_t media_lib_tls_register(media_lib_tls_t *tls_lib);

#ifdef __cplusplus
}
#endif

#endif
