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

#include "media_lib_socket.h"
#include "media_lib_socket_reg.h"
#include "media_lib_common.h"

#ifdef CONFIG_MEDIA_PROTOCOL_LIB_ENABLE
static media_lib_socket_t media_socket_lib;

esp_err_t media_lib_socket_register(media_lib_socket_t *socket_lib)
{
    MEDIA_LIB_DEFAULT_INSTALLER(socket_lib, &media_socket_lib,
                                media_lib_socket_t);
}

int media_lib_socket_accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
    if (media_socket_lib.sock_accept) {
        return media_socket_lib.sock_accept(s, addr, addrlen);
    }
    return ESP_ERR_NOT_SUPPORTED;
}

int media_lib_socket_bind(int s, const struct sockaddr *name, socklen_t namelen)
{
    if (media_socket_lib.sock_bind) {
        return media_socket_lib.sock_bind(s, name, namelen);
    }
    return ESP_ERR_NOT_SUPPORTED;
}

int media_lib_socket_shutdown(int s, int how)
{
    if (media_socket_lib.sock_shutdown) {
        return media_socket_lib.sock_shutdown(s, how);
    }
    return ESP_ERR_NOT_SUPPORTED;
}

int media_lib_socket_close(int s)
{
    if (media_socket_lib.sock_close) {
        return media_socket_lib.sock_close(s);
    }
    return ESP_ERR_NOT_SUPPORTED;
}

int media_lib_socket_connect(int s, const struct sockaddr *name, socklen_t namelen)
{
    if (media_socket_lib.sock_connect) {
        return media_socket_lib.sock_connect(s, name, namelen);
    }
    return ESP_ERR_NOT_SUPPORTED;
}

int media_lib_socket_listen(int s, int backlog)
{
    if (media_socket_lib.sock_listen) {
        return media_socket_lib.sock_listen(s, backlog);
    }
    return ESP_ERR_NOT_SUPPORTED;
}

ssize_t media_lib_socket_recv(int s, void *mem, size_t len, int flags)
{
    if (media_socket_lib.sock_recv) {
        return media_socket_lib.sock_recv(s, mem, len, flags);
    }
    return ESP_ERR_NOT_SUPPORTED;
}

ssize_t media_lib_socket_read(int s, void *mem, size_t len)
{
    if (media_socket_lib.sock_read) {
        return media_socket_lib.sock_read(s, mem, len);
    }
    return ESP_ERR_NOT_SUPPORTED;
}

ssize_t media_lib_socket_readv(int s, const struct iovec *iov, int iovcnt)
{
    if (media_socket_lib.sock_readv) {
        return media_socket_lib.sock_readv(s, iov, iovcnt);
    }
    return ESP_ERR_NOT_SUPPORTED;
}

ssize_t media_lib_socket_recvfrom(int s, void *mem, size_t len, int flags,
                                  struct sockaddr *from, socklen_t *fromlen)
{
    if (media_socket_lib.sock_recvfrom) {
        return media_socket_lib.sock_recvfrom(s, mem, len, flags, from,
                                              fromlen);
    }
    return ESP_ERR_NOT_SUPPORTED;
}

ssize_t media_lib_socket_recvmsg(int s, struct msghdr *message, int flags)
{
    if (media_socket_lib.sock_recvmsg) {
        return media_socket_lib.sock_recvmsg(s, message, flags);
    }
    return ESP_ERR_NOT_SUPPORTED;
}

ssize_t media_lib_socket_send(int s, const void *dataptr, size_t size, int flags)
{
    if (media_socket_lib.sock_send) {
        return media_socket_lib.sock_send(s, dataptr, size, flags);
    }
    return ESP_ERR_NOT_SUPPORTED;
}

ssize_t media_lib_socket_sendmsg(int s, const struct msghdr *message, int flags)
{
    if (media_socket_lib.sock_sendmsg) {
        return media_socket_lib.sock_sendmsg(s, message, flags);
    }
    return ESP_ERR_NOT_SUPPORTED;
}

ssize_t media_lib_socket_sendto(int s, const void *dataptr, size_t size,
                                int flags, const struct sockaddr *to,
                                socklen_t tolen)
{
    if (media_socket_lib.sock_sendto) {
        return media_socket_lib.sock_sendto(s, dataptr, size, flags, to, tolen);
    }
    return ESP_ERR_NOT_SUPPORTED;
}

int media_lib_socket_open(int domain, int type, int protocol)
{
    if (media_socket_lib.sock_open) {
        return media_socket_lib.sock_open(domain, type, protocol);
    }
    return ESP_ERR_NOT_SUPPORTED;
}

ssize_t media_lib_socket_write(int s, const void *dataptr, size_t size)
{
    if (media_socket_lib.sock_write) {
        return media_socket_lib.sock_write(s, dataptr, size);
    }
    return ESP_ERR_NOT_SUPPORTED;
}

ssize_t media_lib_socket_writev(int s, const struct iovec *iov, int iovcnt)
{
    if (media_socket_lib.sock_writev) {
        return media_socket_lib.sock_writev(s, iov, iovcnt);
    }
    return ESP_ERR_NOT_SUPPORTED;
}

int media_lib_socket_select(int maxfdp1, fd_set *readset, fd_set *writeset,
                            fd_set *exceptset, media_lib_timeval *timeout)
{
    if (media_socket_lib.sock_select) {
        return media_socket_lib.sock_select(maxfdp1, readset, writeset,
                                            exceptset, timeout);
    }
    return ESP_ERR_NOT_SUPPORTED;
}

int media_lib_socket_ioctl(int s, long cmd, void *argp) {
    if (media_socket_lib.sock_ioctl) {
        return media_socket_lib.sock_ioctl(s, cmd, argp);
    }
    return ESP_ERR_NOT_SUPPORTED;
}

int media_lib_socket_fcntl(int s, int cmd, int val)
{
    if (media_socket_lib.sock_fcntl) {
        return media_socket_lib.sock_fcntl(s, cmd, val);
    }
    return ESP_ERR_NOT_SUPPORTED;
}

const char *media_lib_socket_inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
    if (media_socket_lib.sock_inet_ntop) {
        return media_socket_lib.sock_inet_ntop(af, src, dst, size);
    }
    return NULL;
}

int media_lib_socket_inet_pton(int af, const char *src, void *dst)
{
    if (media_socket_lib.sock_inet_pton) {
        return media_socket_lib.sock_inet_pton(af, src, dst);
    }
    return ESP_ERR_NOT_SUPPORTED;
}

int media_lib_socket_setsockopt(int s, int level, int optname,
                                const void *opval, socklen_t optlen)
{
    if (media_socket_lib.sock_setsockopt) {
        return media_socket_lib.sock_setsockopt(s, level, optname, opval, optlen);
    }
    return ESP_ERR_NOT_SUPPORTED;
}

int media_lib_socket_getsockopt(int s, int level, int optname,
                                void *opval, socklen_t *optlen)
{
    if (media_socket_lib.sock_getsockopt) {
        return media_socket_lib.sock_getsockopt(s, level, optname, opval, optlen);
    }
    return ESP_ERR_NOT_SUPPORTED;
}

int media_lib_socket_getsockname(int s, struct sockaddr *name, socklen_t *namelen)
{
    if (media_socket_lib.sock_getsockname) {
        return media_socket_lib.sock_getsockname(s, name, namelen);
    }
    return ESP_ERR_NOT_SUPPORTED;
}
#endif
