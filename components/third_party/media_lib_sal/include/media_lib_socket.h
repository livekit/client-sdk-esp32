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

#ifndef MEDIA_LIB_SOCKET_H
#define MEDIA_LIB_SOCKET_H

#include "media_lib_socket_reg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief      Wrapper for socket accept
 * @return     - 0: on success
 *             - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *             - Others: socket accept fail
 */
int media_lib_socket_accept(int s, struct sockaddr *addr, socklen_t *addrlen);

/**
 * @brief      Wrapper for socket bind
 * @return     - 0: on success
 *             - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *             - Others: socket bind fail
 */
int media_lib_socket_bind(int s, const struct sockaddr *name, socklen_t namelen);

/**
 * @brief      Wrapper for socket shutdown
 * @return     - 0: on success
 *             - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *             - Others: socket shutdown fail
 */
int media_lib_socket_shutdown(int s, int how);

/**
 * @brief      Wrapper for socket close
 * @return     - 0: on success
 *             - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *             - Others: socket close fail
 */
int media_lib_socket_close(int s);

/**
 * @brief      Wrapper for socket connect
 * @return     - 0: on success
 *             - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *             - Others: socket connect fail
 */
int media_lib_socket_connect(int s, const struct sockaddr *name, socklen_t namelen);

/**
 * @brief      Wrapper for socket listen
 * @return     - 0: on success
 *             - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *             - Others: listen fail
 */
int media_lib_socket_listen(int s, int backlog);

/**
 * @brief      Wrapper for socket recv
 * @return     - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *             - Others: returned by recvb wrapper function directly
 */
ssize_t media_lib_socket_recv(int s, void *mem, size_t len, int flags);

/**
 * @brief      Wrapper for socket read
 * @return     - ESP_ERR_NOT_SUPPORTED wrapper function not registered
 *             - Others: returned by read wrapper function directly
 */
ssize_t media_lib_socket_read(int s, void *mem, size_t len);

/**
 * @brief      Wrapper for socket readv
 * @return     - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *             - Others: returned by readv wrapper function directly
 */
ssize_t media_lib_socket_readv(int s, const struct iovec *iov, int iovcnt);

/**
 * @brief      Wrapper for socket recvfrom
 * @return     - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *             - Others: returned by recvfrom wrapper function directly
 */
ssize_t media_lib_socket_recvfrom(int s, void *mem, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);

/**
 * @brief      Wrapper for socket recvmsg
 * @return     - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *             - Others: returned by recvmsg wrapper function directly
 */
ssize_t media_lib_socket_recvmsg(int s, struct msghdr *message, int flags);

/**
 * @brief      Wrapper for socket send
 * @return     - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *             - Others: returned by send wrapper function directly
 */
ssize_t media_lib_socket_send(int s, const void *dataptr, size_t size, int flags);

/**
 * @brief      Wrapper for socket sendmsg
 * @return     - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *             - Others: returned by sendmsg wrapper function directly
 */
ssize_t media_lib_socket_sendmsg(int s, const struct msghdr *message, int flags);

/**
 * @brief      Wrapper for socket sendto
 * @return     - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *             - Others: returned by sendto wrapper function directly
 */
ssize_t media_lib_socket_sendto(int s, const void *dataptr, size_t size, int flags, const struct sockaddr *to, socklen_t tolen);

/**
 * @brief      Wrapper for socket open function
 * @return     - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *             - Others: returned by socket open wrapper function directly
 */
int media_lib_socket_open(int domain, int type, int protocol);

/**
 * @brief      Wrapper for socket write
 * @return     - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *             - Others: returned by socket write wrapper function directly
 */
ssize_t media_lib_socket_write(int s, const void *dataptr, size_t size);

/**
 * @brief      Wrapper for socket writev
 * @return     - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *             - Others: returned by writev wrapper function directly
 */
ssize_t media_lib_socket_writev(int s, const struct iovec *iov, int iovcnt);

/**
 * @brief      Wrapper for socket select
 * @return     - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *             - Others: returned by select wrapper function directly
 */
int media_lib_socket_select(int maxfdp1, fd_set *readset, fd_set *writeset, fd_set *exceptset, media_lib_timeval *timeout);

/**
 * @brief      Wrapper for socket ioctl
 * @return     - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *             - Others: returned by ioctl wrapper function directly
 */
int media_lib_socket_ioctl(int s, long cmd, void *argp);

/**
 * @brief      Wrapper for socket fcntl
 * @return     - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *             - Others: returned by fcntl wrapper function directly
 */
int media_lib_socket_fcntl(int s, int cmd, int val);

/**
 * @brief      Wrapper for inet_ntop
 * @return     - not NULL: on success
 *             - NULL: convert to string fail
 */
const char *media_lib_socket_inet_ntop(int af, const void *src, char *dst, socklen_t size);

/**
 * @brief      Wrapper for inet_pton
 * @return     - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *             - Others: returned by inet_pton wrapper function directly
 */
int media_lib_socket_inet_pton(int af, const char *src, void *dst);

/**
 * @brief      Wrapper for setsockopt
 * @return     - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *             - Others: returned by setsockopt wrapper function directly
 */
int media_lib_socket_setsockopt(int s, int level, int optname, const void *opval, socklen_t optlen);

/**
 * @brief      Wrapper for getsockopt
 * @return     - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *             - Others: returned by getsockopt wrapper function directly
 */
int media_lib_socket_getsockopt(int s, int level, int optname, void *opval, socklen_t *optlen);

/**
 * @brief      Wrapper for getsockname
 * @return     - ESP_ERR_NOT_SUPPORTED: wrapper function not registered
 *             - Others: returned by getsockname wrapper function directly
 */
int media_lib_socket_getsockname(int s, struct sockaddr *name, socklen_t *namelen);

#ifdef __cplusplus
}
#endif

#endif
