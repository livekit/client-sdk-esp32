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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Https response data
 */
typedef struct {
    char *data; /*!< Response data */
    int   size; /*!< Response data size */
} http_resp_t;

/**
 * @brief  Https response callback
 *
 * @param[in]  resp  Body content
 * @param[in]  ctx   User context
 */
typedef void (*http_body_t)(http_resp_t *resp, void *ctx);

/**
 * @brief  Https header callback
 *
 * @param[in]  key  Header key
 * @param[in]  key  Header value
 * @param[in]  ctx  User context
 */
typedef void (*http_header_t)(const char *key, const char *value, void *ctx);

/**
 * @brief  Send https requests
 *
 * @note  This API is run in synchronized until response or error returns
 *
 * @param[in]  method     HTTP method to do
 * @param[in]  headers    HTTP headers, headers are array of "Type: Info", last one need set to NULL
 * @param[in]  url        HTTPS URL
 * @param[in]  data       Content data to be sent
 * @param[in]  header_cb  Header callback
 * @param[in]  body       Body callback
 * @param[in]  ctx        User context
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to do https request
 */
int https_send_request(const char *method, char **headers, const char *url, char *data, http_header_t header_cb, http_body_t body_cb, void *ctx);

/**
 * @brief  Do post https request
 *
 * @note  This API will internally call `https_send_request`
 *
 * @param[in]  url        HTTPS URL to post
 * @param[in]  headers    HTTP headers, headers are array of "Type: Info", last one need set to NULL
 * @param[in]  data       Content data to be sent
 * @param[in]  header_cb  Header callback
 * @param[in]  body       Body callback
 * @param[in]  ctx        User context
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to do https request
 */
int https_post(const char *url, char **headers, char *data, http_header_t header_cb, http_body_t body, void *ctx);

#ifdef __cplusplus
}
#endif
