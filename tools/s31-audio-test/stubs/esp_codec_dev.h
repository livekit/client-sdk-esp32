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
/// Opaque host fake; no physical codec is opened by this test.
typedef void *esp_codec_dev_handle_t;
/// Format fields consumed by the renderer's codec-open path.
typedef struct {
    /// PCM frames per second.
    unsigned sample_rate;
    /// Number of interleaved channels.
    unsigned channel;
    /// Width of one channel sample.
    unsigned bits_per_sample;
    /// Active codec channel selection.
    unsigned channel_mask;
} esp_codec_dev_sample_info_t;
/// Match the codec API's success result.
#define ESP_CODEC_DEV_OK 0
/// Record format initialization in the host fake.
int esp_codec_dev_open(esp_codec_dev_handle_t device, esp_codec_dev_sample_info_t *info);
/// Assert that no writer remains active when closing.
int esp_codec_dev_close(esp_codec_dev_handle_t device);
/// Simulate a blocking codec write and count delivered bytes.
int esp_codec_dev_write(esp_codec_dev_handle_t device, void *data, int size);
/// Accept the renderer's volume restoration without hardware access.
int esp_codec_dev_set_out_vol(esp_codec_dev_handle_t device, int volume);
