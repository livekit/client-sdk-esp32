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

#include "media_lib_adapter.h"
#include "media_lib_socket_reg.h"

#ifdef CONFIG_MEDIA_PROTOCOL_LIB_ENABLE
static ssize_t _readv(int s, const struct iovec *iov, int iovcnt)
{
    return 0;
}

static ssize_t _recvmsg(int s, struct msghdr *message, int flags)
{
    return recvmsg(s, message, flags);
}

static const char *_inet_ntop(int af, const void *src, char *dst,
                              socklen_t size)
{
    return lwip_inet_ntop(af, src, dst, size);
}

static int _inet_pton(int af, const char *src, void *dst)
{
    return lwip_inet_pton(af, src, dst);
}

static int _setsockopt(int s, int level, int optname, const void *opval,
                       socklen_t optlen)
{
    return setsockopt(s, level, optname, opval, optlen);
}

static int _getsockopt(int s, int level, int optname, void *opval,
                       socklen_t *optlen)
{
    return getsockopt(s, level, optname, opval, optlen);
}

static int _getsockname(int s, struct sockaddr *name, socklen_t *namelen)
{
    return getsockname(s, name, namelen);
}

static int _select(int maxfdp1, fd_set *readset, fd_set *writeset, fd_set *exceptset, media_lib_timeval *timeout) {
    int ret;
    struct timeval tm = {
        .tv_sec = timeout->tv_sec,
        .tv_usec = timeout->tv_usec,
    };
    ret = lwip_select(maxfdp1, readset, writeset, exceptset, &tm);
    return ret;
}

esp_err_t media_lib_add_default_socket_adapter(void)
{
    media_lib_socket_t sock_lib = {
        .sock_accept = lwip_accept,
        .sock_bind = lwip_bind,
        .sock_shutdown = lwip_shutdown,
        .sock_close = lwip_close,
        .sock_connect = lwip_connect,
        .sock_listen = lwip_listen,
        .sock_recv = lwip_recv,
        .sock_read = lwip_read,
        .sock_readv = _readv,
        .sock_recvfrom = lwip_recvfrom,
        .sock_recvmsg = _recvmsg,
        .sock_send = lwip_send,
        .sock_sendmsg = lwip_sendmsg,
        .sock_sendto = lwip_sendto,
        .sock_open = lwip_socket,
        .sock_write = lwip_write,
        .sock_writev = lwip_writev,
        .sock_select = _select,
        .sock_ioctl = lwip_ioctl,
        .sock_fcntl = lwip_fcntl,
        .sock_inet_ntop = _inet_ntop,
        .sock_inet_pton = _inet_pton,
        .sock_setsockopt = _setsockopt,
        .sock_getsockopt = _getsockopt,
        .sock_getsockname = _getsockname,
    };
    return media_lib_socket_register(&sock_lib);
}
#endif
