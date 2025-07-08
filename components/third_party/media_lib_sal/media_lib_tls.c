/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2022 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#include "media_lib_tls.h"
#include "media_lib_tls_reg.h"
#include "media_lib_common.h"

#ifdef CONFIG_MEDIA_PROTOCOL_LIB_ENABLE
static media_lib_tls_t media_tls_lib;

esp_err_t media_lib_tls_register(media_lib_tls_t *tls_lib)
{
    MEDIA_LIB_DEFAULT_INSTALLER(tls_lib, &media_tls_lib, media_lib_tls_t);
}

media_lib_tls_handle_t media_lib_tls_new(const char *hostname, int hostlen, int port, const media_lib_tls_cfg_t *cfg)
{
    if (media_tls_lib.tls_new) {
        return media_tls_lib.tls_new(hostname, hostlen, port, cfg);
    }
    return NULL;
}

media_lib_tls_handle_t media_lib_tls_new_server(int fd, const media_lib_tls_server_cfg_t *cfg)
{
    if (media_tls_lib.tls_new_server) {
        return media_tls_lib.tls_new_server(fd, cfg);
    }
    return NULL;
}

int media_lib_tls_get_bytes_avail(media_lib_tls_handle_t tls)
{
    if (media_tls_lib.tls_get_bytes_avail) {
        return media_tls_lib.tls_get_bytes_avail(tls);
    }

    return ESP_ERR_NOT_SUPPORTED;
}

int media_lib_tls_write(media_lib_tls_handle_t tls, const void *data, size_t datalen)
{
    if (media_tls_lib.tls_write) {
        return media_tls_lib.tls_write(tls, data, datalen);
    }

    return ESP_ERR_NOT_SUPPORTED;
}

int media_lib_tls_read(media_lib_tls_handle_t tls, void *data, size_t datalen)
{
    if (media_tls_lib.tls_read) {
        return media_tls_lib.tls_read(tls, data, datalen);
    }

    return ESP_ERR_NOT_SUPPORTED;
}

int media_lib_tls_getsockfd(media_lib_tls_handle_t tls)
{
    if (media_tls_lib.tls_getsockfd) {
        return media_tls_lib.tls_getsockfd(tls);
    }
    return ESP_ERR_NOT_SUPPORTED;
}

int media_lib_tls_delete(media_lib_tls_handle_t tls)
{
    if (media_tls_lib.tls_delete) {
        return media_tls_lib.tls_delete(tls);
    }
    return ESP_ERR_NOT_SUPPORTED;
}

#endif
