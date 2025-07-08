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

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Capture error code
 */
typedef enum {
    ESP_CAPTURE_ERR_OK,
    ESP_CAPTURE_ERR_NO_MEM        = -1,
    ESP_CAPTURE_ERR_INVALID_ARG   = -2,
    ESP_CAPTURE_ERR_NOT_SUPPORTED = -3,
    ESP_CAPTURE_ERR_NOT_FOUND     = -4,
    ESP_CAPTURE_ERR_NOT_ENOUGH,
    ESP_CAPTURE_ERR_TIMEOUT       = -5,
    ESP_CAPTURE_ERR_INVALID_STATE = -6,
    ESP_CAPTURE_ERR_INTERNAL      = -7,
    ESP_CAPTURE_ERR_NO_RESOURCES  = -8,
} esp_capture_err_t;

/**
 * @brief  Capture codec types
 */
typedef enum {
    ESP_CAPTURE_CODEC_TYPE_NONE,
    ESP_CAPTURE_CODEC_TYPE_AUDIO = 0x20,
    ESP_CAPTURE_CODEC_TYPE_PCM,     /*!< Audio PCM codec */
    ESP_CAPTURE_CODEC_TYPE_G711A,   /*!< Audio G711-ALaw codec */
    ESP_CAPTURE_CODEC_TYPE_G711U,   /*!< Audio G711-ULaw codec */
    ESP_CAPTURE_CODEC_TYPE_OPUS,    /*!< Audio OPUS codec */
    ESP_CAPTURE_CODEC_TYPE_AAC,     /*!< Audio AAC codec */
    ESP_CAPTURE_CODEC_TYPE_VIDEO = 0x40,
    ESP_CAPTURE_CODEC_TYPE_H264,    /*!< Video H264 codec */
    ESP_CAPTURE_CODEC_TYPE_MJPEG,   /*!< Video JPEG codec */
    ESP_CAPTURE_CODEC_TYPE_RGB565,  /*!< Video RGB565 codec */
    ESP_CAPTURE_CODEC_TYPE_RGB8888, /*!< Video RGB8888 codec */
    ESP_CAPTURE_CODEC_TYPE_YUV420P, /*!< Video YUV420 progressive codec */
    ESP_CAPTURE_CODEC_TYPE_YUV422P, /*!< Video YUV422 progressive codec */
    ESP_CAPTURE_CODEC_TYPE_YUV420,  /*!< Video YUV420 codec */
    ESP_CAPTURE_CODEC_TYPE_YUV422,  /*!< Video YUV422 codec */
} esp_capture_codec_type_t;

/**
 * @brief  Capture stream type
 */
typedef enum {
    ESP_CAPTURE_STREAM_TYPE_NONE,
    ESP_CAPTURE_STREAM_TYPE_AUDIO, /*!< Audio stream type */
    ESP_CAPTURE_STREAM_TYPE_VIDEO, /*!< Video stream type */
    ESP_CAPTURE_STREAM_TYPE_MUXER, /*!< Mux stream type */
} esp_capture_stream_type_t;

/**
 * @brief  Capture frame information
 */
typedef struct {
    esp_capture_stream_type_t stream_type; /*!< Capture stream type */
    uint32_t                  pts;         /*!< Stream frame presentation timestamp (unit ms) */
    uint8_t                  *data;        /*!< Stream frame data pointer */
    int                       size;        /*!< Stream frame data size */
} esp_capture_stream_frame_t;

/**
 * @brief  Capture audio information
 */
typedef struct {
    esp_capture_codec_type_t codec;           /*!< Audio codec */
    uint32_t                 sample_rate;     /*!< Audio sample rate */
    uint8_t                  channel;         /*!< Audio channel */
    uint8_t                  bits_per_sample; /*!< Audio bits per sample */
} esp_capture_audio_info_t;

/**
 * @brief  Capture video information
 */
typedef struct {
    esp_capture_codec_type_t codec;  /*!< Video codec */
    uint32_t                 width;  /*!< Video width */
    uint32_t                 height; /*!< Video height */
    uint8_t                  fps;    /*!< Video frames per second */
    uint32_t                 bitrate;
    uint32_t                 gop;
} esp_capture_video_info_t;

/**
 * @brief  Capture sink configuration
 */
typedef struct {
    esp_capture_audio_info_t audio_info; /*!< Audio sink information */
    esp_capture_video_info_t video_info; /*!< Video sink information */
} esp_capture_sink_cfg_t;

/**
 * @brief  Capture region
 */
typedef struct {
    uint32_t x;      /*!< Region x position */
    uint32_t y;      /*!< Region y position */
    uint32_t width;  /*!< Region width */
    uint32_t height; /*!< Region height */
} esp_capture_rgn_t;

#ifdef __cplusplus
}
#endif
