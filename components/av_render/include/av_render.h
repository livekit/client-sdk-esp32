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

#include "av_render_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  AV render handle
 */
typedef void *av_render_handle_t;

/**
 * @brief  AV render sync mode
 */
typedef enum {
    AV_RENDER_SYNC_NONE,         /*!< No sync, audio video run separately */
    AV_RENDER_SYNC_FOLLOW_AUDIO, /*!< Sync according audio position */
    AV_RENDER_SYNC_FOLLOW_TIME,  /*!< Sync according system time */
} av_render_sync_mode_t;

/**
 * @brief  AV render configuration
 */
typedef struct {
    audio_render_handle_t audio_render;           /*!< Audio render handle */
    video_render_handle_t video_render;           /*!< Video render handle */
    av_render_sync_mode_t sync_mode;              /*!< AV sync mode */
    uint32_t              audio_raw_fifo_size;    /*!< Audio Decoder fifo size setting. if set, will create audio decode thread to decode data */
    uint32_t              video_raw_fifo_size;    /*!< Video Decoder fifo size setting. If set, will create video decode thread to decode data */
    uint32_t              audio_render_fifo_size; /*!< Audio Render fifo size setting. If set, will create audio render thread to render audio */
    uint32_t              video_render_fifo_size; /*!< Video  Render fifo size setting. If set, will create video render thread to render audio */
    bool                  quit_when_eos;          /*!< When received stream eos, quit thread or just wait for new data */
    bool                  allow_drop_data;        /*!< Allow drop data when decode or render too slow */
    bool                  pause_render_only;      /*!< If call `av_render_pause` only pause render thread or pause all thread */
    bool                  pause_on_first_frame;   /*!< Whether automatically pause when render receive first frame */
    void                 *ctx;                    /*!< User context */
    bool                  video_cvt_in_render;    /*!< Convert color in render*/
} av_render_cfg_t;

/**
 * @brief  AV render event type
 */
typedef enum {
    AV_RENDER_EVENT_NONE,             /*!< No event happen */
    AV_RENDER_EVENT_AUDIO_RENDERED,   /*!< Audio is rendered */
    AV_RENDER_EVENT_VIDEO_RENDERED,   /*!< Video is rendered */
    AV_RENDER_EVENT_AUDIO_EOS,        /*!< Audio stream eos */
    AV_RENDER_EVENT_VIDEO_EOS,        /*!< Video stream eos */
    AV_RENDER_EVENT_AUDIO_DECODE_ERR, /*!< Audio decode error */
    AV_RENDER_EVENT_VIDEO_DECODE_ERR, /*!< Video decode error */
} av_render_event_t;

/**
 * @brief  AV render fifo statistics
 */
typedef struct {
    uint32_t duration;         /*!< Fifo duration */
    int      q_num;            /*!< Queue number */
    int      data_size;        /*!< Data size */
    int      render_q_num;     /*!< Render queue number */
    int      render_data_size; /*!< Render queue data number */
} av_render_fifo_stat_t;

/**
 * @brief  AV render fifo configuration
 */
typedef struct {
    uint32_t raw_fifo_size;    /*!< Decoder fifo size, if set to 0 will not create decode thread */
    uint32_t render_fifo_size; /*!< Render fifo size, if set to 0 will not create render thread */
} av_render_fifo_cfg_t;

/**
 * @brief  AV render event callback
 *
 * @param[in]  event  AV render event
 * @param[in]  ctx    User context
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to process event
 */
typedef int (*av_render_event_cb)(av_render_event_t event, void *ctx);

/**
 * @brief  Data pool free callback
 *
 * @param[in]  data  Data to be freed
 * @param[in]  ctx   Data pool context
 */
typedef void (*av_render_pool_data_free)(void *data, void *ctx);

/**
 * @brief  open AV render
 *
 * @param[in]  cfg  AV render configuration
 * @param[in]  ctx  User context
 *
 * @return
 *       - NULL    No resource
 *       - Others  AV render handle
 */
av_render_handle_t av_render_open(av_render_cfg_t *cfg);

/**
 * @brief  Set event callback for AV render
 *
 * @param[in]  render  AV render handle
 * @param[in]  cb      AV render event callback
 * @param[in]  ctx     User context
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to set event callback
 */
int av_render_set_event_cb(av_render_handle_t render, av_render_event_cb cb, void *ctx);

/**
 * @brief  Configuration for audio fifo for AV render
 *
 * @param[in]  render    AV render handle
 * @param[in]  fifo_cfg  Audio fifo configuration
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to config audio fifo
 */
int av_render_config_audio_fifo(av_render_handle_t render, av_render_fifo_cfg_t *fifo_cfg);

/**
 * @brief  Add audio stream for AV render
 *
 * @param[in]  render      AV render handle
 * @param[in]  audio_info  Audio stream information
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to add audio stream
 */
int av_render_add_audio_stream(av_render_handle_t render, av_render_audio_info_t *audio_info);

/**
 * @brief  Set audio fixed frame info
 *
 * @note  This API need set after add audio stream
 *
 * @param[in]  render      AV render handle
 * @param[in]  frame_info  Audio stream information
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to set fixed frame info
 */
int av_render_set_fixed_frame_info(av_render_handle_t render, av_render_audio_frame_info_t *frame_info);

/**
 * @brief  Configuration for video fifo for AV render
 *
 * @param[in]  render    AV render handle
 * @param[in]  fifo_cfg  Video fifo configuration
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to config video fifo
 */
int av_render_config_video_fifo(av_render_handle_t render, av_render_fifo_cfg_t *fifo_cfg);

/**
 * @brief  Add video stream for AV render
 *
 * @param[in]  render      AV render handle
 * @param[in]  video_info  Video stream information
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to add video stream
 */
int av_render_add_video_stream(av_render_handle_t render, av_render_video_info_t *video_info);

/**
 * @brief  Data pool setting for AV render
 *
 * @note  When input audio and video data is in data pool, to avoid extra copy, need need provide pool free API
 *        When AV render not use it, it will call provided free API to release the pool data
 *
 * @param[in]  render    AV render handle
 * @param[in]  free      API to free data pool
 * @param[in]  pool_ctx  Data pool context
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to set data pool
 */
int av_render_use_data_pool(av_render_handle_t render, av_render_pool_data_free free, void *pool_ctx);

/**
 * @brief  Add audio data for AV render
 *
 * @param[in]  render      AV render handle
 * @param[in]  audio_data  Audio data
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to add audio data
 */
int av_render_add_audio_data(av_render_handle_t render, av_render_audio_data_t *audio_data);

/**
 * @brief  Add video data for AV render
 *
 * @param[in]  render      AV render handle
 * @param[in]  video_data  Video data
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to add video data
 */
int av_render_add_video_data(av_render_handle_t render, av_render_video_data_t *video_data);

/**
 * @brief  Check audio fifo enough
 *
 * @param[in]  render      AV render handle
 * @param[in]  audio_data  Audio data
 *
 * @return
 *       - true   Fifo is enough
 *       - false  Not enough fifo for input audio data
 */
bool av_render_audio_fifo_enough(av_render_handle_t render, av_render_audio_data_t *audio_data);

/**
 * @brief  Check video fifo enough
 *
 * @param[in]  render      AV render handle
 * @param[in]  video_data  Video data
 *
 * @return
 *       - true   Fifo is enough
 *       - false  Not enough fifo for input video data
 */
bool av_render_video_fifo_enough(av_render_handle_t render, av_render_video_data_t *video_data);

/**
 * @brief  Get audio fifo level
 *
 * @param[in]  render     AV render handle
 * @param[in]  fifo_stat  Audio fifo status
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to get audio fifo level
 */
int av_render_get_audio_fifo_level(av_render_handle_t render, av_render_fifo_stat_t *fifo_stat);

/**
 * @brief  Get video fifo level
 *
 * @param[in]  render     AV render handle
 * @param[in]  fifo_stat  Video fifo status
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to get video fifo level
 */
int av_render_get_video_fifo_level(av_render_handle_t render, av_render_fifo_stat_t *fifo_stat);

/**
 * @brief  Pause for AV render
 *
 * @param[in]  render  AV render handle
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to pause
 */
int av_render_pause(av_render_handle_t render, bool pause);

/**
 * @brief  Set playback speed for AV render
 *
 * @note  Currently only allow to set for sync mode is `AV_RENDER_SYNC_FOLLOW_AUDIO` and have audio stream
 *
 * @param[in]  render  AV render handle
 * @param[in]  speed   Playback speed
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to pause
 */
int av_render_set_speed(av_render_handle_t render, float speed);

/**
 * @brief  Flush for AV render
 *
 * @note  Flush will flush all pending data in decoder or render fifo
 *        Use can call `av_render_add_audio_data` or `av_render_add_video_data` to play new data
 *
 * @param[in]  render  AV render handle
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to pause
 */
int av_render_flush(av_render_handle_t render);

/**
 * @brief  Set video start pts
 *
 * @note  When video start pts is set, if decoded pts less than video start pts, data will de dropped
 *
 * @param[in]  render     AV render handle
 * @param[in]  video_pts  Video start pts
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to set
 */
int av_render_set_video_start_pts(av_render_handle_t render, uint32_t video_pts);

/**
 * @brief  Get current render presentation time
 *
 * @param[in]  render     AV render handle
 * @param[in]  video_pts  Video start pts
 *
 * @return
 *       - 0       On success
 *       - Others  Fail to set
 */
int av_render_get_render_pts(av_render_handle_t h, uint32_t *pts);

/**
 * @brief  Query for AV render
 *
 * @note  This API is used for debug only
 *
 * @param[in]  render  AV render handle
 *
 * @return
 *       - ESP_MEDIA_ERR_INVALID_ARG  Invalid argument
 *       - ESP_MEDIA_ERR_OK           On success
 */
int av_render_query(av_render_handle_t h);

/**
 * @brief  Dump data for AV render
 *
 * @note  This API is used for debug only
 *
 * @param[in]  h     AV render handle
 * @param[in]  mask  Dump mask
 *
 * @return
 *       - ESP_MEDIA_ERR_INVALID_ARG  Invalid argument
 *       - ESP_MEDIA_ERR_OK           On success
 */
void av_render_dump(av_render_handle_t h, uint8_t mask);

/**
 * @brief  Reset AV render
 *
 * @note  When do reset it will stop the audio and video stream
 *        After reset, user can call `av_render_add_audio_stream` or `av_render_add_video_stream` to play new stream
 *
 * @param[in]  h     AV render handle
 * @param[in]  mask  Dump mask
 *
 * @return
 *       - ESP_MEDIA_ERR_INVALID_ARG  Invalid argument
 *       - ESP_MEDIA_ERR_OK           On success
 */
int av_render_reset(av_render_handle_t render);

/**
 * @brief  Close AV render
 *
 * @param[in]  render  AV render handle
 *
 * @return
 *       - ESP_MEDIA_ERR_INVALID_ARG  Invalid argument
 *       - ESP_MEDIA_ERR_OK           On success
 */
int av_render_close(av_render_handle_t render);

#ifdef __cplusplus
}
#endif
