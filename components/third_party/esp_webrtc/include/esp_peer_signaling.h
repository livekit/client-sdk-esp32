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

#include "esp_peer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Signaling ICE information
 */
typedef struct {
    esp_peer_ice_server_cfg_t server_info;  /*!< STUN/Relay server information (optional if set by user directly) */
    bool                      is_initiator; /*!< ICE roles, when as initiator also means being a controlling role */
} esp_peer_signaling_ice_info_t;

/**
 * @brief  Signaling handle
 */
typedef void *esp_peer_signaling_handle_t;

/**
 * @brief  Signaling message type
 */
typedef enum {
    ESP_PEER_SIGNALING_MSG_NONE,       /*!< Signaling message type none */
    ESP_PEER_SIGNALING_MSG_SDP,        /*!< SDP message type (SDP may contain candidate in it) */
    ESP_PEER_SIGNALING_MSG_CANDIDATE,  /*!< Candidate message type */
    ESP_PEER_SIGNALING_MSG_BYE,        /*!< Bye message type */
    ESP_PEER_SIGNALING_MSG_CUSTOMIZED, /*!< Customized message type */
} esp_peer_signaling_msg_type_t;

typedef struct {
    esp_peer_signaling_msg_type_t type; /*!< Signaling message type */
    uint8_t                      *data; /*!< Signaling message data */
    int                           size; /*!< Signaling message size */
} esp_peer_signaling_msg_t;

/**
 * @brief  Signaling configuration
 */
typedef struct {
    /**
     * @brief  Event callback for ICE information
     * @param[in]  info  Pointer to the ICE information
     * @param[in]  ctx   User context
     * @return           Status code indicating success or failure.
     */
    int (*on_ice_info)(esp_peer_signaling_ice_info_t* info, void* ctx);

    /**
     * @brief  Event callback for connected event
     * @param[in]  ctx  User context
     * @return          Status code indicating success or failure.
     */
    int (*on_connected)(void* ctx);

    /**
     * @brief  Message callback
     * @param[in]  msg  Pointer to signaling message
     * @return          Status code indicating success or failure.
     */
    int (*on_msg)(esp_peer_signaling_msg_t* msg, void* ctx);

    /**
     * @brief  Event callback for closed event
     * @param[in]  ctx  User context
     * @return          Status code indicating success or failure.
     */
    int (*on_close)(void* ctx);

    char* signal_url;  /*!< Signaling server URL */
    void* extra_cfg;   /*!< Extra configuration for special signaling server */
    int   extra_size;  /*!< Size of extra configuration */
    void* ctx;         /*!< User context */
} esp_peer_signaling_cfg_t;

 /**
 * @brief  Signaling interface
*/
typedef struct {
    /**
     * @brief  Start signaling
     * @param[in]   cfg  Signaling configuration
     * @param[out]  sig  Signaling handle
     * @return           Status code indicating success or failure.
    */
    int (*start)(esp_peer_signaling_cfg_t* cfg, esp_peer_signaling_handle_t* sig);

    /**
     * @brief  Send message to signaling server
     * @param[in]  sig  Signaling handle
     * @param[in]  msg  Message to be sent
     * @return          Status code indicating success or failure.
    */
    int (*send_msg)(esp_peer_signaling_handle_t sig, esp_peer_signaling_msg_t* msg);

    /**
     * @brief  Stop signaling
     * @param[in]  sig  Signaling handle
     * @return          Status code indicating success or failure.
    */
    int (*stop)(esp_peer_signaling_handle_t sig);
} esp_peer_signaling_impl_t;

/**
 * @brief  Start signaling
 *
 * @param[in]   cfg   Signaling configuration
 * @param[in]   impl  Implement of signaling interface
 * @param[out]  sig   Signaling handle
 *
 * @return
 *       - ESP_PEER_ERR_NONE         Start signaling success
 *       - ESP_PEER_ERR_INVALID_ARG  Invalid argument
 *       - ESP_PEER_ERR_NO_MEM       Not enough memory
 */
int esp_peer_signaling_start(esp_peer_signaling_cfg_t *cfg, const esp_peer_signaling_impl_t *impl, esp_peer_signaling_handle_t *sig);

/**
 * @brief  Send message to signaling server
 *
 * @param[in]  sig  Signaling handle
 * @param[in]  msg  Message to be sent
 *
 * @return
 *       - ESP_PEER_ERR_NONE         Send message success
 *       - ESP_PEER_ERR_INVALID_ARG  Invalid argument
 *       - Others                    Fail to send message
 */
int esp_peer_signaling_send_msg(esp_peer_signaling_handle_t sig, esp_peer_signaling_msg_t *msg);

/**
 * @brief  Stop signaling
 *
 * @param[in]  sig  Signaling handle
 *
 * @return
 *       - ESP_PEER_ERR_NONE         Stop signaling success
 *       - ESP_PEER_ERR_INVALID_ARG  Invalid argument
 *       - Others                    Fail to stop
 */
int esp_peer_signaling_stop(esp_peer_signaling_handle_t sig);

#ifdef __cplusplus
}
#endif
