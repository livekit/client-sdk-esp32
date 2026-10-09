/*
 * Copyright 2026 LiveKit, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include <stdbool.h>
#include <stdint.h>
/// Opaque renderer handle matching the production callback interface.
typedef void *audio_render_handle_t;
/// PCM format passed to the renderer under test.
typedef struct {
    /// Number of interleaved channels.
    uint8_t channel;
    /// Width of one channel sample.
    uint8_t bits_per_sample;
    /// PCM frames per second.
    uint32_t sample_rate;
} av_render_audio_frame_info_t;
/// Borrowed PCM frame copied by the renderer before write returns.
typedef struct {
    /// Presentation timestamp supplied by the caller.
    uint32_t pts;
    /// Caller-owned interleaved PCM.
    uint8_t *data;
    /// Payload length in bytes.
    int size;
    /// End-of-stream marker.
    bool eos;
} av_render_audio_frame_t;
/// Production callback surface needed to compile the actual renderer.
typedef struct {
    /// Allocate renderer state from configuration.
    audio_render_handle_t (*init)(void *, int);
    /// Start a stream with the supplied PCM format.
    int (*open)(audio_render_handle_t, av_render_audio_frame_info_t *);
    /// Queue borrowed PCM for asynchronous playback.
    int (*write)(audio_render_handle_t, av_render_audio_frame_t *);
    /// Report queued playback delay.
    int (*get_latency)(audio_render_handle_t, uint32_t *);
    /// Return the active PCM format.
    int (*get_frame_info)(audio_render_handle_t, av_render_audio_frame_info_t *);
    /// Request a playback speed multiplier.
    int (*set_speed)(audio_render_handle_t, float);
    /// Stop the writer before releasing codec access.
    int (*close)(audio_render_handle_t);
    /// Release renderer state after close.
    void (*deinit)(audio_render_handle_t);
} audio_render_ops_t;
/// Initialization envelope consumed by the host allocation stub.
typedef struct {
    /// Renderer callbacks copied by value.
    audio_render_ops_t ops;
    /// Borrowed initialization configuration.
    void *cfg;
    /// Configuration length in bytes.
    int cfg_size;
} audio_render_cfg_t;
/// Invoke initialization through the same callback used by av_render.
audio_render_handle_t audio_render_alloc_handle(audio_render_cfg_t *cfg);
