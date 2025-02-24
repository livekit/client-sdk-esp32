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
 * @brief  Peer state
 */
typedef enum {
    ESP_PEER_STATE_CLOSED              = 0, /*!< Closed */
    ESP_PEER_STATE_DISCONNECTED        = 1, /*!< Disconnected */
    ESP_PEER_STATE_NEW_CONNECTION      = 2, /*!< New connection comming */
    ESP_PEER_STATE_PAIRING             = 3, /*!< Under candidate pairing */
    ESP_PEER_STATE_PAIRED              = 4, /*!< Candidate pairing success */
    ESP_PEER_STATE_CONNECTING          = 5, /*!< Building connection with peer */
    ESP_PEER_STATE_CONNECTED           = 6, /*!< Connected with peer */
    ESP_PEER_STATE_CONNECT_FAILED      = 7, /*!< Connect failed */
    ESP_PEER_STATE_DATA_CHANNEL_OPENED = 8, /*!< Data channel is opened */
    ESP_PEER_STATE_DATA_CHANNEL_CLOSED = 9, /*!< Data channel is closed */
} esp_peer_state_t;

/**
 * @brief  Peer video codec
 */
typedef enum {
    ESP_PEER_VIDEO_CODEC_NONE  = 0, /*!< Invalid video codec type */
    ESP_PEER_VIDEO_CODEC_H264  = 1, /*!< H264 video codec */
    ESP_PEER_VIDEO_CODEC_MJPEG = 2, /*!< MJPEG video codec */
} esp_peer_video_codec_t;

/**
 * @brief  Peer audio codec
 */
typedef enum {
    ESP_PEER_AUDIO_CODEC_NONE  = 0, /*!< Invalid audio codec type */
    ESP_PEER_AUDIO_CODEC_G711A = 1, /*!< G711-alaw(PCMA) audio codec */
    ESP_PEER_AUDIO_CODEC_G711U = 2, /*!< G711-ulaw(PCMU) audio codec */
    ESP_PEER_AUDIO_CODEC_OPUS  = 3, /*!< OPUS audio codec */
} esp_peer_audio_codec_t;

/**
 * @brief  Data channel type
 */
typedef enum {
    ESP_PEER_DATA_CHANNEL_NONE   = 0, /*!< Invalid type */
    ESP_PEER_DATA_CHANNEL_DATA   = 1, /*!< Data type */
    ESP_PEER_DATA_CHANNEL_STRING = 2, /*!< String type */
} esp_peer_data_channel_type_t;

/**
 * @brief  Media transmission direction
 */
typedef enum {
    ESP_PEER_MEDIA_DIR_NONE      = 0,
    ESP_PEER_MEDIA_DIR_SEND_ONLY = (1 << 0), /*!< Send only */
    ESP_PEER_MEDIA_DIR_RECV_ONLY = (1 << 1), /*!< Receive only */
    ESP_PEER_MEDIA_DIR_SEND_RECV = ESP_PEER_MEDIA_DIR_SEND_ONLY | ESP_PEER_MEDIA_DIR_RECV_ONLY,
    /*!< Send and receive both */
} esp_peer_media_dir_t;

/**
 * @brief  ICE transport policy
 */
typedef enum {
    ESP_PEER_ICE_TRANS_POLICY_ALL   = 0, /*!< All ICE candidates will be used for pairing */
    ESP_PEER_ICE_TRANS_POLICY_RELAY = 1, /*!< Only relay ICE candidates will be used for pairing */
} esp_peer_ice_trans_policy_t;

/**
 * @brief  Video stream information
 */
typedef struct {
    esp_peer_video_codec_t codec;  /*!< Video codec */
    int                    width;  /*!< Video width */
    int                    height; /*!< Video height */
    int                    fps;    /*!< Video fps */
} esp_peer_video_stream_info_t;

/**
 * @brief  Audio stream information
 */
typedef struct {
    esp_peer_audio_codec_t codec;       /*!< Audio codec */
    uint32_t               sample_rate; /*!< Audio sample rate */
    uint8_t                channel;     /*!< Audio channel */
} esp_peer_audio_stream_info_t;

/**
 * @brief  Video frame information
 */
typedef struct {
    uint32_t pts;  /*!< Video frame presentation timestamp */
    uint8_t *data; /*!< Video frame data pointer */
    int      size; /*!< Video frame data size */
} esp_peer_video_frame_t;

/**
 * @brief  Audio frame information
 */
typedef struct {
    uint32_t pts;  /*!< Audio frame presentation timestamp */
    uint8_t *data; /*!< Audio frame data pointer */
    int      size; /*!< Audio frame data size */
} esp_peer_audio_frame_t;

/**
 * @brief  Data frame information
 */
typedef struct {
    esp_peer_data_channel_type_t type; /*!< Data channel type */
    uint8_t                     *data; /*!< Pointer to data to be sent through data channel */
    int                          size; /*!< Data size */
} esp_peer_data_frame_t;

/**
 * @brief  Peer message type
 */
typedef enum {
    ESP_PEER_MSG_TYPE_NONE,      /*!< None message type */
    ESP_PEER_MSG_TYPE_SDP,       /*!< SDP message type */
    ESP_PEER_MSG_TYPE_CANDIDATE, /*!< ICE candidate message type */
} esp_peer_msg_type_t;

/**
 * @brief  Peer message
 */
typedef struct {
    esp_peer_msg_type_t type; /*!< Message type */
    uint8_t            *data; /*!< Message data */
    int                 size; /*!< Message data size */
} esp_peer_msg_t;

/**
 * @brief  Peer handle
 */
typedef void *esp_peer_handle_t;

/**
 * @brief  Peer configuration
 */
typedef struct {
    esp_peer_ice_server_cfg_t   *server_lists;        /*< ICE server list */
    uint8_t                      server_num;          /*!< Number of ICE server */
    esp_peer_role_t              role;                /*!< Peer role */
    esp_peer_ice_trans_policy_t  ice_trans_policy;    /*!< ICE transport policy */
    esp_peer_audio_stream_info_t audio_info;          /*!< Audio stream information */
    esp_peer_video_stream_info_t video_info;          /*!< Video stream information */
    esp_peer_media_dir_t         audio_dir;           /*!< Audio transmission direction */
    esp_peer_media_dir_t         video_dir;           /*!< Video transmission direction */
    bool                         enable_data_channel; /*!< Enable data channel */
    void                        *extra_cfg;           /*!< Extra configuration */
    int                          extra_size;          /*!< Extra configuration size */
    void                        *ctx;                 /*!< User context */

    /**
     * @brief  Event callback for state
     * @param[in]  state  Current peer state
     * @param[in]  ctx    User context
     * @return            Status code indicating success or failure.
     */
    int (*on_state)(esp_peer_state_t state, void* ctx);

    /**
     * @brief  Message callback
     * @param[in]  info  Pointer to peer message
     * @param[in]  ctx   User context
     * @return           Status code indicating success or failure.
     */
    int (*on_msg)(esp_peer_msg_t* info, void* ctx);

    /**
     * @brief  Peer video stream information callback
     * @param[in]  info  Video stream information
     * @param[in]  ctx   User context
     * @return           Status code indicating success or failure.
     */
    int (*on_video_info)(esp_peer_video_stream_info_t* info, void* ctx);

    /**
     * @brief  Peer audio stream information callback
     * @param[in]  info  Audio stream information
     * @param[in]  ctx   User context
     * @return           Status code indicating success or failure.
     */
    int (*on_audio_info)(esp_peer_audio_stream_info_t* info, void* ctx);

    /**
     * @brief  Peer audio frame callback
     * @param[in]  frame  Audio frame information
     * @param[in]  ctx    User context
     * @return            Status code indicating success or failure.
     */
    int (*on_audio_data)(esp_peer_audio_frame_t* frame, void* ctx);

    /**
     * @brief  Peer video frame callback
     * @param[in]  frame  Video frame information
     * @param[in]  ctx    User context
     * @return            Status code indicating success or failure.
     */
    int (*on_video_data)(esp_peer_video_frame_t* frame, void* ctx);

    /**
     * @brief  Peer data frame callback
     * @param[in]  frame  Data frame information
     * @param[in]  ctx    User context
     * @return            Status code indicating success or failure.
     */
    int (*on_data)(esp_peer_data_frame_t* frame, void* ctx);
} esp_peer_cfg_t;

/**
 * @brief  Peer connection interface
 */
typedef struct {
    /**
     * @brief  Open peer connection
     * @param[in]   cfg   Peer configuration
     * @param[out]  peer  Peer handle
     * @return            Status code indicating success or failure.
     */
    int (*open)(esp_peer_cfg_t* cfg, esp_peer_handle_t* peer);

    /**
     * @brief  Create a new conenction
     * @param[in]   peer  Peer handle
     * @return            Status code indicating success or failure.
     */
    int (*new_connection)(esp_peer_handle_t peer);

    /**
     * @brief  Update ICE information
     * @param[in]   peer        Peer handle
     * @param[in]   servers     ICE Server settings
     * @param[in]   server_num  Number of ICE servers
     * @return            Status code indicating success or failure.
     */
    int (*update_ice_info)(esp_peer_handle_t peer, esp_peer_role_t role, esp_peer_ice_server_cfg_t* server, int server_num);

    /**
     * @brief  Send message to peer
     * @param[in]   peer  Peer handle
     * @param[in]   msg   Message to be sent
     * @return            Status code indicating success or failure.
     */
    int (*send_msg)(esp_peer_handle_t peer, esp_peer_msg_t* msg);

    /**
     * @brief  Send video frame data to peer
     * @param[in]   peer   Peer handle
     * @param[in]   frame  Video frame to be sent
     * @return             Status code indicating success or failure.
     */
    int (*send_video)(esp_peer_handle_t peer, esp_peer_video_frame_t* frame);

    /**
     * @brief  Send audio frame data to peer
     * @param[in]   peer   Peer handle
     * @param[in]   frame  Audio frame to be sent
     * @return             Status code indicating success or failure.
     */
    int (*send_audio)(esp_peer_handle_t peer, esp_peer_audio_frame_t* frame);

    /**
     * @brief  Send data frame data to peer
     * @param[in]   peer   Peer handle
     * @param[in]   frame  Data frame to be sent
     * @return             Status code indicating success or failure.
     */
    int (*send_data)(esp_peer_handle_t peer, esp_peer_data_frame_t* frame);

    /**
     * @brief  Peer main loop
     * @note  Peer connection need handle peer status change, receive stream data in this loop
     *        Or create a thread to handle these things and synchronize with this loop
     * @param[in]   peer   Peer handle
     * @return             Status code indicating success or failure.
     */
    int (*main_loop)(esp_peer_handle_t peer);

    /**
     * @brief  Disconnected with peer
     * @param[in]   peer   Peer handle
     * @return             Status code indicating success or failure.
     */
    int (*disconnect)(esp_peer_handle_t peer);

    /**
     * @brief  Query peer status
     * @param[in]   peer   Peer handle
     */
    void (*query)(esp_peer_handle_t peer);

    /**
     * @brief  Close peer connection and release related resources
     * @param[in]   peer   Peer handle
     * @return             Status code indicating success or failure.
     */
    int (*close)(esp_peer_handle_t peer);
} esp_peer_ops_t;

/**
 * @brief  Open peer connection
 *
 * @param[in]   cfg   Peer configuration
 * @param[in]   ops   Peer connection implementation
 * @param[out]  peer  Peer handle
 *
 * @return
 *       - ESP_PEER_ERR_NONE         Open peer connection success
 *       - ESP_PEER_ERR_INVALID_ARG  Invalid argument
 *       - ESP_PEER_ERR_NOT_SUPPORT  Not support
 */
int esp_peer_open(esp_peer_cfg_t *cfg, const esp_peer_ops_t *ops, esp_peer_handle_t *handle);

/**
 * @brief  Create new conenction
 *
 * @note  After new connection is created, It will try gather ICE candidate from ICE servers.
 *        And report local SDP to let user send to signaling server.
 *
 * @param[in]  peer  Peer handle
 *
 * @return
 *       - ESP_PEER_ERR_NONE         Open peer connection success
 *       - ESP_PEER_ERR_INVALID_ARG  Invalid argument
 *       - ESP_PEER_ERR_NOT_SUPPORT  Not support
 */
int esp_peer_new_connection(esp_peer_handle_t handle);

/**
 * @brief  Update ICE server information
 *
 * @note  After new connection is created, It will try gather ICE candidate from ICE servers.
 *        And report local SDP to let user send to signaling server.
 *
 * @param[in]  peer  Peer handle
 *
 * @return
 *       - ESP_PEER_ERR_NONE         Open peer connection success
 *       - ESP_PEER_ERR_INVALID_ARG  Invalid argument
 *       - ESP_PEER_ERR_NOT_SUPPORT  Not support
 */
int esp_peer_update_ice_info(esp_peer_handle_t handle, esp_peer_role_t role, esp_peer_ice_server_cfg_t* server, int server_num);

/**
 * @brief  Send message to peer
 *
 * @param[in]  peer  Peer handle
 * @param[in]  msg   Message to send to peer
 *
 * @return
 *       - ESP_PEER_ERR_NONE         Open peer connection success
 *       - ESP_PEER_ERR_INVALID_ARG  Invalid argument
 *       - ESP_PEER_ERR_NOT_SUPPORT  Not support
 */
int esp_peer_send_msg(esp_peer_handle_t peer, esp_peer_msg_t *msg);

/**
 * @brief  Send video data to peer
 *
 * @param[in]  peer   Peer handle
 * @param[in]  frame  Video frame data
 *
 * @return
 *       - ESP_PEER_ERR_NONE         Open peer connection success
 *       - ESP_PEER_ERR_INVALID_ARG  Invalid argument
 *       - ESP_PEER_ERR_NOT_SUPPORT  Not support
 */
int esp_peer_send_video(esp_peer_handle_t peer, esp_peer_video_frame_t *frame);

/**
 * @brief  Send audio data to peer
 *
 * @param[in]  peer   Peer handle
 * @param[in]  frame  Audio frame data
 *
 * @return
 *       - ESP_PEER_ERR_NONE         Open peer connection success
 *       - ESP_PEER_ERR_INVALID_ARG  Invalid argument
 *       - ESP_PEER_ERR_NOT_SUPPORT  Not support
 */
int esp_peer_send_audio(esp_peer_handle_t peer, esp_peer_audio_frame_t *info);

/**
 * @brief  Send data through data channel to peer
 *
 * @param[in]  peer   Peer handle
 * @param[in]  frame  Video frame data
 *
 * @return
 *       - ESP_PEER_ERR_NONE         Open peer connection success
 *       - ESP_PEER_ERR_INVALID_ARG  Invalid argument
 *       - ESP_PEER_ERR_NOT_SUPPORT  Not support
 */
int esp_peer_send_data(esp_peer_handle_t peer, esp_peer_data_frame_t *frame);

/**
 * @brief  Run peer connection main loop
 *
 * @note  This loop need to be call repeatedly
 *        It handle peer connection status change also receive stream data
 *        Currently default peer realization have no extra thread internally, all is triggered in this loop
 *
 * @param[in]  peer  Peer handle
 *
 * @return
 *       - ESP_PEER_ERR_NONE         Open peer connection success
 *       - ESP_PEER_ERR_INVALID_ARG  Invalid argument
 *       - ESP_PEER_ERR_NOT_SUPPORT  Not support
 */
int esp_peer_main_loop(esp_peer_handle_t peer);

/**
 * @brief  Disconnect peer connection
 *
 * @note  Disconnect will try to close socket which communicate with peer and signaling server
 *        If user want to continue to listen for peer connect in
 *        User can call `esp_peer_new_connection` so that it will retry to gather ICE candidate and report local SDP
 *        So that new peer can connection in through offered SDP from signaling server.
 *
 * @param[in]  peer  Peer handle
 *
 * @return
 *       - ESP_PEER_ERR_NONE         Open peer connection success
 *       - ESP_PEER_ERR_INVALID_ARG  Invalid argument
 *       - ESP_PEER_ERR_NOT_SUPPORT  Not support
 */
int esp_peer_disconnect(esp_peer_handle_t handle);

/**
 * @brief  Query of peer connection
 *
 * @note  This API is for debug usage only
 *
 * @param[in]  peer  Peer handle
 *
 * @return
 *       - ESP_PEER_ERR_NONE         Open peer connection success
 *       - ESP_PEER_ERR_INVALID_ARG  Invalid argument
 */
int esp_peer_query(esp_peer_handle_t handle);

/**
 * @brief  Close peer connection
 *
 * @note  Close peer connection will do `esp_peer_disconnect` firstly then release all resource occupied by peer realization.
 *
 * @param[in]  peer  Peer handle
 *
 * @return
 *       - ESP_PEER_ERR_NONE         Open peer connection success
 *       - ESP_PEER_ERR_INVALID_ARG  Invalid argument
 *       - ESP_PEER_ERR_NOT_SUPPORT  Not support
 */
int esp_peer_close(esp_peer_handle_t handle);

#ifdef __cplusplus
}
#endif
