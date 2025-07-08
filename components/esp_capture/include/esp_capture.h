
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

#include "esp_capture_types.h"
#include "esp_capture_path_if.h"
#include "esp_capture_audio_src_if.h"
#include "esp_capture_video_src_if.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Capture handle
 */
typedef void *esp_capture_handle_t;

/**
 * @brief  Capture path handle
 * @note  A capture system may contain multiple capture paths
 *        Each path can have its special audio and video settings and can be configured to connect to muxer or not.
 */
typedef void *esp_capture_path_handle_t;

/**
 * @brief  Capture sync mode
 */
typedef enum {
    ESP_CAPTURE_SYNC_MODE_NONE,   /*!< Audio and video without sync */
    ESP_CAPTURE_SYNC_MODE_SYSTEM, /*!< Audio and video synced with system time */
    ESP_CAPTURE_SYNC_MODE_AUDIO,  /*!< Video sync follow audio */
} esp_capture_sync_mode_t;

/**
 * @brief  Capture configuration
 */
typedef struct {
    esp_capture_sync_mode_t     sync_mode;    /*!< Capture sync mode */
    esp_capture_audio_src_if_t *audio_src;    /*!< Capture audio source interface */
    esp_capture_video_src_if_t *video_src;    /*!< Capture video source interface */
    esp_capture_path_if_t      *capture_path; /*!< Capture path interface */
} esp_capture_cfg_t;

/**
 * @brief  Capture run type
 * @note  Capture run type is used to control capture path behavior
 */
typedef enum {
    ESP_CAPTURE_RUN_TYPE_DISABLE = 0, /*!< Disable capture path, not run anymore */
    ESP_CAPTURE_RUN_TYPE_ALWAYS  = 1, /*!< Enable capture path, run always */
    ESP_CAPTURE_RUN_TYPE_ONCE    = 2  /*!< Enable capture once, use scenario like capture one image */
} esp_capture_run_type_t;

/**
 * @brief  Capture muxer configuration
 */
typedef enum {
    ESP_CAPTURE_MUXER_TYPE_NONE, /*!< None muxer type */
    ESP_CAPTURE_MUXER_TYPE_TS,   /*!< TS muxer type */
    ESP_CAPTURE_MUXER_TYPE_MP4,  /*!< MP4 muxer type */
} esp_capture_muxer_type_t;

/**
 * @brief  Capture muxer mask
 * @note  Capture muxer mask is used control whether enable audio or video muxer
 */
typedef enum {
    ESP_CAPTURE_MUXER_MASK_ALL   = 0, /*!< Mux for both audio and video */
    ESP_CAPTURE_MUXER_MASK_AUDIO = 1, /*!< Mux for audio stream only */
    ESP_CAPTURE_MUXER_MASK_VIDEO = 2, /*!< Mux for video stream only */
} esp_capture_muxer_mask_t;

/**
 * @brief  Capture  muxer configuration
 */
typedef struct {
    esp_capture_muxer_type_t muxer_type; /* Muxer type */
    esp_capture_muxer_mask_t muxer_mask; /* Muxer masks to muxer */
    int (*slice_cb)(char *file_path, int len, int slice_idx);
    /* File path to save file */
    uint32_t muxer_cache_size;   /* Cache size to hold muxer output data */
    bool     capture_muxer_data; /* Whether capture muxer output data */
    bool     muxer_only;
    /* When set means user do not want to get audio sink or video sink output data
    Data is feed to muxer and consumed by muxer only */
} esp_capture_muxer_cfg_t;

/**
 * @brief  Open capture
 *
 * @param[in]   cfg      Capture configuration
 * @param[out]  capture  Pointer to output capture system handle
 *
 * @return
 *       - ESP_CAPTURE_ERR_OK            Success to open capture
 *       - ESP_CAPTURE_ERR_INVALID_ARG   Invalid input argument
 *       - ESP_CAPTURE_ERR_NO_MEM        Not enough memory for capture instance
 *       - ESP_CAPTURE_ERR_NO_RESOURCES  No related resources
 *
 */
int esp_capture_open(esp_capture_cfg_t *cfg, esp_capture_handle_t *capture);

/**
 * @brief  Setup capture path to use certain sink settings
 *
 * @note  Only support setup path when path not existed, or path disabled or not started
 *        Setup to an existed path will get the existed path handle
 *
 * @param[in]   capture      Capture handle
 * @param[in]   path         Path to be added
 * @param[in]   sink_info    Sink setting for audio and video stream
 * @param[out]  path_handle  Pointer to output capture path handle
 *
 * @return
 *       - ESP_CAPTURE_ERR_OK             Success to open capture
 *       - ESP_CAPTURE_ERR_INVALID_ARG    Invalid input argument
 *       - ESP_CAPTURE_ERR_NO_MEM         Not enough memory for capture instance
 *       - ESP_CAPTURE_ERR_INVALID_STATE  Path already added
 *       - ESP_CAPTURE_ERR_NOT_SUPPORTED  Not supported to do path add (path interface not provided)
 */
int esp_capture_setup_path(esp_capture_handle_t capture, esp_capture_path_type_t path,
                           esp_capture_sink_cfg_t *sink_info, esp_capture_path_handle_t *path_handle);

/**
 * @brief  Add muxer to capture path
 *
 * @param[in]  h          Capture path handle
 * @param[in]  muxer_cfg  Muxer configuration
 *
 * @return
 *       - ESP_CAPTURE_ERR_OK             Success to open capture
 *       - ESP_CAPTURE_ERR_INVALID_ARG    Invalid input argument
 *       - ESP_CAPTURE_ERR_INVALID_STATE  Muxer already added or can not add after capture system started
 */
int esp_capture_add_muxer_to_path(esp_capture_path_handle_t h, esp_capture_muxer_cfg_t *muxer_cfg);

/**
 * @brief  Add overlay to capture path
 *
 * @param[in]  h        Capture path handle
 * @param[in]  overlay  Overlay interface
 *
 * @return
 *       - ESP_CAPTURE_ERR_OK             Success to open capture
 *       - ESP_CAPTURE_ERR_INVALID_ARG    Invalid input argument
 *       - ESP_CAPTURE_ERR_NOT_SUPPORTED  Path interface not provided
 */
int esp_capture_add_overlay_to_path(esp_capture_path_handle_t h, esp_capture_overlay_if_t *overlay);

/**
 * @brief  Enable muxer for capture path
 *
 * @param[in]  h       Capture path handle
 * @param[in]  enable  Enable muxer or not
 *
 * @return
 *       - ESP_CAPTURE_ERR_OK             Success to open capture
 *       - ESP_CAPTURE_ERR_INVALID_ARG    Invalid input argument
 *       - ESP_CAPTURE_ERR_NOT_SUPPORTED  Path interface not provided
 */
int esp_capture_enable_muxer(esp_capture_path_handle_t h, bool enable);

/**
 * @brief  Enable overlay for capture path
 *
 * @note  Use can enable overlay at any time, even after `esp_capture_start`
 *
 * @param[in]  h       Capture path handle
 * @param[in]  enable  Enable overlay or not
 *
 * @return
 *       - ESP_CAPTURE_ERR_OK             Success to open capture
 *       - ESP_CAPTURE_ERR_INVALID_ARG    Invalid input argument
 *       - ESP_CAPTURE_ERR_NOT_SUPPORTED  Path interface not provided
 */
int esp_capture_enable_overlay(esp_capture_path_handle_t h, bool enable);

/**
 * @brief  Enable capture path
 *
 * @note  Use can enable capture path at any time, even after `esp_capture_start`
 *
 * @param[in]  h         Capture path handle
 * @param[in]  run_type  Run type for capture path
 *
 * @return
 *       - ESP_CAPTURE_ERR_OK             Success to open capture
 *       - ESP_CAPTURE_ERR_INVALID_ARG    Invalid input argument
 *       - ESP_CAPTURE_ERR_NOT_SUPPORTED  Path interface not provided
 */
int esp_capture_enable_path(esp_capture_path_handle_t h, esp_capture_run_type_t run_type);

/**
 * @brief  Start capture system
 *
 * @note  If capture system contain multiple capture path, all paths will be started
 *
 * @param[in]  capture  Capture system handle
 *
 * @return
 *       - ESP_CAPTURE_ERR_OK           Success to start capture
 *       - ESP_CAPTURE_ERR_INVALID_ARG  Invalid input argument
 */
int esp_capture_start(esp_capture_handle_t capture);

/**
 * @brief  Set stream bitrate for capture path
 *
 * @note  If capture system contain multiple capture path, all paths will be started
 *
 * @param[in]  h            Capture path handle
 * @param[in]  stream_type  Capture stream type
 * @param[in]  bitrate      Stream bitrate to set
 *
 * @return
 *       - ESP_CAPTURE_ERR_OK             Success to open capture
 *       - ESP_CAPTURE_ERR_INVALID_ARG    Invalid input argument
 *       - ESP_CAPTURE_ERR_NOT_SUPPORTED  Path interface not provided
 */
int esp_capture_set_path_bitrate(esp_capture_path_handle_t h, esp_capture_stream_type_t stream_type, uint32_t bitrate);

/**
 * @brief  Acquire stream data from capture path
 *
 * @note  Stream data is internally managed by capture system, use need not provide memory to hold it
 *        Meanwhile after use done, must call `esp_capture_release_stream` to release stream data
 *        Use need set `frame->stream_type` to specify which stream type to acquire
 *
 * @param[in]      h        Capture path handle
 * @param[in,out]  frame    Capture frame information
 * @param[in]      no_wait  Whether need wait for frame data, set to `true` to not wait
 *
 * @return
 *       - ESP_CAPTURE_ERR_OK             Success to open capture
 *       - ESP_CAPTURE_ERR_INVALID_ARG    Invalid input argument
 *       - ESP_CAPTURE_ERR_NOT_SUPPORTED  Path interface not provided
 */
int esp_capture_acquire_path_frame(esp_capture_path_handle_t h, esp_capture_stream_frame_t *frame, bool no_wait);

/**
 * @brief  Release stream data from capture path
 *
 * @note  Use need make sure frame data, size, stream_type is same as the one acquired from `esp_capture_acquire_path_frame`
 *
 * @param[in]  h      Capture path handle
 * @param[in]  frame  Capture frame information
 *
 * @return
 *       - ESP_CAPTURE_ERR_OK             Success to open capture
 *       - ESP_CAPTURE_ERR_INVALID_ARG    Invalid input argument
 *       - ESP_CAPTURE_ERR_NOT_SUPPORTED  Wrong stream type
 *       - ESP_CAPTURE_ERR_INVALID_STATE  Capture path not enable yet
 */
int esp_capture_release_path_frame(esp_capture_path_handle_t h, esp_capture_stream_frame_t *frame);

/**
 * @brief  Stop capture
 *
 * @note  All capture paths will be stopped
 *
 * @param[in]  capture  Capture path handle
 *
 * @return
 *       - ESP_CAPTURE_ERR_OK           Success to open capture
 *       - ESP_CAPTURE_ERR_INVALID_ARG  Invalid input argument
 *
 */
int esp_capture_stop(esp_capture_handle_t capture);

/**
 * @brief  Close capture
 *
 * @note  The whole capture system will be destroyed, all related capture paths will be destroyed also
 *
 * @param[in]  capture  Capture path handle
 *
 * @return
 *       - ESP_CAPTURE_ERR_OK           Success to open capture
 *       - ESP_CAPTURE_ERR_INVALID_ARG  Invalid input argument
 *
 */
int esp_capture_close(esp_capture_handle_t capture);

#ifdef __cplusplus
}
#endif
