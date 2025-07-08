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

#include "msg_q.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Shared queue configuration
 */
typedef struct {
    uint8_t user_count;                          /*!< Output user counts */
    uint8_t q_count;                             /*!< Maximum queue count for output user */
    int     item_size;                           /*!< Item size to fill into queue */
    void *(*get_frame_data)(void *item);         /*!< Function to get frame data (used to distinguish frame) */
    int (*release_frame)(void *item, void *ctx); /*!< Function to release frame */
    void *ctx;                                   /*!< Input context for release frame */
    bool  use_external_q;                        /*!< Input context for release frame */
} share_q_cfg_t;

/**
 * @brief  Shared queue handle
 *
 * @note  This shared queue is designed for distributing frame data. It has one input
 *        and multiple output consumers. The data is shared by reference and is only
 *        released when all consumers have finished using the frame. When input data
 *        arrives, the frame is pushed to all active output queues. Each consumer retrieves
 *        frame data from the queue and releases it when done. The shared queue tracks
 *        the release actions of consumers and uses a reference count to determine when
 *        to release the actual frame data.
 */
typedef struct share_q_t *share_q_handle_t;

/**
 * @brief  Create share queue
 *
 * @param[in]  cfg  Share queue configuration
 * @return
 *       - NULL    No resources for share queue
 *       - Others  Share queue handle
 *
 */
share_q_handle_t share_q_create(share_q_cfg_t *cfg);

/**
 * @brief  Set external queue for share queue by index
 *
 * @param[in]  q       Share queue handle
 * @param[in]  index   Index of the queue
 * @param[in]  handle  Message queue handle
 *
 * @return
 *       - 0   On success
 *       - -1  Invalid input arguments
 *
 */
int share_q_set_external(share_q_handle_t q, uint8_t index, msg_q_handle_t handle);

/**
 * @brief  Receive frame from share queue by index
 *
 * @param[in]   q      Share queue handle
 * @param[in]   index  Index of the queue
 * @param[out]  frame  Frame information to be filled
 *
 * @return
 *       - 0   On success
 *       - -1  Invalid input arguments
 *
 */
int share_q_recv(share_q_handle_t q, uint8_t index, void *frame);

/**
 * @brief  Receive all frames from share queue and release input frame accordingly
 *
 * @param[in]   q      Share queue handle
 * @param[out]  frame  Frame information to be filled
 *
 * @return
 *       - 0   On success
 *       - -1  Invalid input arguments
 *
 */
int share_q_recv_all(share_q_handle_t q, void *frame);

/**
 * @brief  Enable or disable shared queue output by index
 *
 * @note  Enabling or disabling can happen at any time. When disabled, input frames
 *        will not be inserted into the queue of the specified output index.
 *
 * @param[in]  q       Shared queue handle
 * @param[in]  index   Output index
 * @param[in]  enable  Enable or disable
 *
 * @return
 *       - 0   On success
 *       - -1  Invalid input arguments
 *
 */
int share_q_enable(share_q_handle_t q, uint8_t index, bool enable);

/**
 * @brief  Check whether output queue of the specified index is enabled
 *
 * @param[in]  q      Shared queue handle
 * @param[in]  index  Output index
 *
 * @return
 *       - true   Queue of the specified index is enabled
 *       - false  Queue of the specified index is disabled
 *
 */
bool share_q_is_enabled(share_q_handle_t q, uint8_t index);

/**
 * @brief  Add frame into share queue
 *
 * @param[in]  q     Shared queue handle
 * @param[in]  item  Frame to be inserted into queues
 *
 * @return
 *       - 0   On success
 *       - -1  Fail to add frame
 *
 */
int share_q_add(share_q_handle_t q, void *item);

/**
 * @brief  Release frame
 *
 * @param[in]  q     Shared queue handle
 * @param[in]  item  Frame to be released
 *
 * @return
 *       - 0   On success
 *       - -1  Fail to release frame
 *
 */
int share_q_release(share_q_handle_t q, void *item);

/**
 * @brief  Destroy share queue
 *
 * @param[in]  q  Shared queue handle
 *
 */
void share_q_destroy(share_q_handle_t q);

#ifdef __cplusplus
}
#endif
