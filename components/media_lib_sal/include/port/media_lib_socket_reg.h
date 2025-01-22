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

#ifndef MEDIA_LIB_SOCKET_REG_H
#define MEDIA_LIB_SOCKET_REG_H

#include "lwip/sockets.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief      Use short timeval to compatible with IDF higher version which support 64bits timer
 */
typedef struct {
    int tv_sec;
    int tv_usec;
} media_lib_timeval;

typedef int (*__media_lib_socket_accept)(int s, struct sockaddr *addr, socklen_t *addrlen);
typedef int (*__media_lib_socket_bind)(int s, const struct sockaddr *name, socklen_t namelen);
typedef int (*__media_lib_socket_shutdown)(int s, int how);
typedef int (*__media_lib_socket_close)(int s);
typedef int (*__media_lib_socket_connect)(int s, const struct sockaddr *name, socklen_t namelen);
typedef int (*__media_lib_socket_listen)(int s, int backlog);
typedef ssize_t (*__media_lib_socket_recv)(int s, void *mem, size_t len, int flags);
typedef ssize_t (*__media_lib_socket_read)(int s, void *mem, size_t len);
typedef ssize_t (*__media_lib_socket_readv)(int s, const struct iovec *iov, int iovcnt);
typedef ssize_t (*__media_lib_socket_recvfrom)(int s, void *mem, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);
typedef ssize_t (*__media_lib_socket_recvmsg)(int s, struct msghdr *message, int flags);
typedef ssize_t (*__media_lib_socket_send)(int s, const void *dataptr, size_t size, int flags);
typedef ssize_t (*__media_lib_socket_sendmsg)(int s, const struct msghdr *message, int flags);
typedef ssize_t (*__media_lib_socket_sendto)(int s, const void *dataptr, size_t size, int flags, const struct sockaddr *to, socklen_t tolen);
typedef int (*__media_lib_socket_open)(int domain, int type, int protocol);
typedef ssize_t (*__media_lib_socket_write)(int s, const void *dataptr, size_t size);
typedef ssize_t (*__media_lib_socket_writev)(int s, const struct iovec *iov, int iovcnt);
typedef int (*__media_lib_socket_select)(int maxfdp1, fd_set *readset, fd_set *writeset, fd_set *exceptset, media_lib_timeval *timeout);
typedef int (*__media_lib_socket_ioctl)(int s, long cmd, void *argp);
typedef int (*__media_lib_socket_fcntl)(int s, int cmd, int val);
typedef const char *(*__media_lib_socket_inet_ntop)(int af, const void *src, char *dst, socklen_t size);
typedef int (*__media_lib_socket_inet_pton)(int af, const char *src, void *dst);
typedef int (*__media_lib_socket_setsockopt)(int s, int level, int optname, const void *opval, socklen_t optlen);
typedef int (*__media_lib_socket_getsockopt)(int s, int level, int optname, void *opval, socklen_t *optlen);
typedef int (*__media_lib_socket_getsockname)(int s, struct sockaddr *name, socklen_t *namelen);

/**
 * @brief      Socket Wrapper Functions Group
 */
typedef struct {
    __media_lib_socket_accept      sock_accept;      /*!< Socket accept Func Pointer */
    __media_lib_socket_bind        sock_bind;        /*!< Socket bind Func Pointer */
    __media_lib_socket_shutdown    sock_shutdown;    /*!< Socket shutdown Func Pointer */
    __media_lib_socket_close       sock_close;       /*!< Socket close Func Pointer */
    __media_lib_socket_connect     sock_connect;     /*!< Socket connect Func Pointer */
    __media_lib_socket_listen      sock_listen;      /*!< Socket listen Func Pointer */
    __media_lib_socket_recv        sock_recv;        /*!< Socket recv Func Pointer */
    __media_lib_socket_read        sock_read;        /*!< Socket read Func Pointer */
    __media_lib_socket_readv       sock_readv;       /*!< Socket readb Func Pointer */
    __media_lib_socket_recvfrom    sock_recvfrom;    /*!< Socket recvfrom Func Pointer */
    __media_lib_socket_recvmsg     sock_recvmsg;     /*!< Socket recvmsg Func Pointer */
    __media_lib_socket_send        sock_send;        /*!< Socket send Func Pointer */
    __media_lib_socket_sendmsg     sock_sendmsg;     /*!< Socket sendmsg Func Pointer */
    __media_lib_socket_sendto      sock_sendto;      /*!< Socket sendto Func Pointer */
    __media_lib_socket_open        sock_open;        /*!< Socket open Func Pointer */
    __media_lib_socket_write       sock_write;       /*!< Socket write Func Pointer */
    __media_lib_socket_writev      sock_writev;      /*!< Socket writev Func Pointer */
    __media_lib_socket_select      sock_select;      /*!< Socket select Func Pointer */
    __media_lib_socket_ioctl       sock_ioctl;       /*!< Socket ioctl Func Pointer */
    __media_lib_socket_fcntl       sock_fcntl;       /*!< Socket fcntl Func Pointer */
    __media_lib_socket_inet_ntop   sock_inet_ntop;   /*!< Socket inet_ntop Func Pointer */
    __media_lib_socket_inet_pton   sock_inet_pton;   /*!< Socket inet_pton Func Pointer */
    __media_lib_socket_setsockopt  sock_setsockopt;  /*!< Socket setspckopt Func Pointer */
    __media_lib_socket_getsockopt  sock_getsockopt;  /*!< Socket getsockopt Func Pointer */
    __media_lib_socket_getsockname sock_getsockname; /*!< Socket getsockname Func Pointer */
} media_lib_socket_t;

/**
 * @brief     Register Socket lib functions for media library
 *
 * @param      socket_lib  Socket wrapper function lists
 *
 * @return
 *             - ESP_OK: on success
 *             - ESP_ERR_INVALID_ARG: some members of socket lib not set
 */
esp_err_t media_lib_socket_register(media_lib_socket_t *socket_lib);

#ifdef __cplusplus
}
#endif

#endif
