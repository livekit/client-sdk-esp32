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
#include <stdlib.h>
#include <string.h>
#include "media_lib_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Audio render stream type
 */
typedef enum {
    AV_RENDER_STREAM_TYPE_NONE,  /*!< Invalid stream type */
    AV_RENDER_STREAM_TYPE_AUDIO, /*!< Audio stream type */
    AV_RENDER_STREAM_TYPE_VIDEO  /*!< Video stream type */
} av_render_stream_type_t;

/**
 * @brief  Video codec type
 */
typedef enum {
    AV_RENDER_VIDEO_CODEC_NONE,   /*!< Invalid video type */
    AV_RENDER_VIDEO_CODEC_H264,   /*!< H264 video type */
    AV_RENDER_VIDEO_CODEC_MJPEG,  /*!< MJPEG video type */
    AV_RENDER_VIDEO_CODEC_YUV420, /*!< YUV420 video type */
    AV_RENDER_VIDEO_CODEC_YUV422, /*!< YUV422 video type */
    AV_RENDER_VIDEO_CODEC_RGB565, /*!< RGB565 video type */
} av_render_video_codec_t;

/**
 * @brief  Audio codec type
 */
typedef enum {
    AV_RENDER_AUDIO_CODEC_NONE,   /*!< Invalid audio type */
    AV_RENDER_AUDIO_CODEC_AAC,    /*!< AAC audio type */
    AV_RENDER_AUDIO_CODEC_MP3,    /*!< MP3 audio type */
    AV_RENDER_AUDIO_CODEC_PCM,    /*!< PCM audio type */
    AV_RENDER_AUDIO_CODEC_ADPCM,  /*!< IMA-ADPCM audio type */
    AV_RENDER_AUDIO_CODEC_G711A,  /*!< G711 alaw audio type */
    AV_RENDER_AUDIO_CODEC_G711U,  /*!< G711 ulaw audio type */
    AV_RENDER_AUDIO_CODEC_AMRNB,  /*!< AMRNB audio type */
    AV_RENDER_AUDIO_CODEC_AMRWB,  /*!< AMRWB audio type */
    AV_RENDER_AUDIO_CODEC_OPUS,   /*!< OPUS audio type */
    AV_RENDER_AUDIO_CODEC_FLAC,   /*!< FLAC audio type */
    AV_RENDER_AUDIO_CODEC_VORBIS, /*!< VORBIS audio type */
    AV_RENDER_AUDIO_CODEC_ALAC,   /*!< ALAC audio type */
} av_render_audio_codec_t;

/**
 * @brief Audio stream information
 */
typedef struct {
    av_render_audio_codec_t codec;           /*!< Audio codec type */
    uint8_t                 channel;         /*!< Audio channel */
    uint8_t                 bits_per_sample; /*!< Audio bits per sample */
    uint32_t                sample_rate;     /*!< Audio sample rate */
    uint32_t                aac_no_adts : 1;    /*!< AAC no adts */
    void                   *codec_spec_info; /*!< Audio codec specified information */
    int                     spec_info_len;   /*!< Audio codec specified information length */
} av_render_audio_info_t;

/**
 * @brief Audio frame information
 */
typedef struct {
    uint8_t  channel;         /*!< Audio channel */
    uint8_t  bits_per_sample; /*!< Audio bits per sample */
    uint32_t sample_rate;     /*!< Audio sample rate */
} av_render_audio_frame_info_t;

/**
 * @brief Audio frame
 */
typedef struct {
    uint32_t pts;   /*!< Audio PTS (unit ms) */
    uint8_t *data;  /*!< Audio data */
    int      size;  /*!< Audio data size */
    bool     eos;   /*!< End of stream data */
} av_render_audio_frame_t;

/**
 * @brief video render frame type
 */
typedef enum {
    AV_RENDER_VIDEO_RAW_TYPE_NONE,      /*!< Invalid video render frame type */
    AV_RENDER_VIDEO_RAW_TYPE_YUV422,    /*!< YUV422 frame type */
    AV_RENDER_VIDEO_RAW_TYPE_YUV420,    /*!< YUV420 frame type */
    AV_RENDER_VIDEO_RAW_TYPE_RGB565,    /*!< RGB565 frame type */
    AV_RENDER_VIDEO_RAW_TYPE_RGB565_BE, /*!< RGB565 bigedian frame type */
    AV_RENDER_VIDEO_RAW_TYPE_MAX,       /*!< Maximum of video render frame type */
} av_render_video_frame_type_t;

/**
 * @brief video render frame information
 */
typedef struct {
    av_render_video_frame_type_t type;   /*!< Frame type */
    uint16_t                     width;  /*!< Video width */
    uint16_t                     height; /*!< Video height */
    uint8_t                      fps;    /*!< Video framerate per second */
} av_render_video_frame_info_t;

/**
 * @brief video render frame data
 */
typedef struct {
    uint32_t pts;  /*!< PTS of frame data */
    uint8_t *data; /*!< Frame data */
    int      size; /*!< Frame data size */
    bool     eos;  /*!< End of stream data */
} av_render_video_frame_t;

/**
 * @brief video render frame buffer
 *
 * @note  When use frame buffer, means decode output directly put to frame buffer without extra buffering
 */
typedef struct {
    uint8_t *data;  /*!< Frame buffer data */
    int      size;  /*!< Frame buffer size */
} av_render_frame_buffer_t;

/**
 * @brief Video information
 */
typedef struct {
    av_render_video_codec_t codec;           /*!< Video codec type */
    uint16_t                width;           /*!< Video width */
    uint16_t                height;          /*!< Video height */
    uint8_t                 fps;             /*!< Video framerate per second */
    void                   *codec_spec_info; /*!< Video codec specified info,
                                                 For H264 need to provide SPS and PPS information*/
    int                     spec_info_len;   /*!< Video codec specified information length */
} av_render_video_info_t;

/**
 * @brief  Audio data
 */
typedef struct {
    uint32_t pts;  /*!< PTS of audio data */
    uint8_t *data; /*!< Audio data pointer */
    uint32_t size; /*!< Audio data size */
    bool     eos;  /*!< End of stream data*/
} av_render_audio_data_t;

/**
 * @brief  Video data
 */
typedef struct {
    uint32_t pts;       /*!< PTS of video data */
    bool     key_frame; /*!< Whether it is key frame */
    uint8_t *data;      /*!< Video data pointer */
    uint32_t size;      /*!< Video data size */
    bool     eos;       /*!< End of stream data */
} av_render_video_data_t;

/**
 * @brief  audio render handle
 */
typedef void *audio_render_handle_t;

/**
 * @brief  video render handle
 */
typedef void *video_render_handle_t;

#ifdef __cplusplus
}
#endif
