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

#include "esp_check.h"
#include "esp_log.h"
#include "av_render_default.h"
#include "esp_audio_dec_default.h"
#include "esp_audio_enc_default.h"
#include "esp_capture_defaults.h"
#include "esp_capture_sink.h"

#include "board.h"
#include "media.h"
#include "sdkconfig.h"
#if CONFIG_IDF_TARGET_ESP32S31
#include "livekit_s31_audio_render.h"
#include "livekit_s31_aec.h"
#endif

static const char *TAG = "media";

#define NULL_CHECK(pointer, message) \
    ESP_RETURN_ON_FALSE(pointer != NULL, -1, TAG, message)

typedef struct {
    esp_capture_sink_handle_t capturer_handle;
    esp_capture_audio_src_if_t *audio_source;
} capture_system_t;

typedef struct {
    audio_render_handle_t audio_renderer;
    av_render_handle_t av_renderer_handle;
} renderer_system_t;

static capture_system_t capturer_system;
static renderer_system_t renderer_system;

#if CONFIG_LK_CUSTOM_S31_BOARD_KORVO && !CONFIG_LK_EXAMPLE_ENABLE_AEC
static esp_capture_err_t (*korvo_read_frame)(esp_capture_audio_src_if_t *,
                                           esp_capture_stream_frame_t *);

static esp_capture_err_t read_korvo_microphone(esp_capture_audio_src_if_t *src,
                                             esp_capture_stream_frame_t *frame)
{
    if (frame == NULL || frame->data == NULL || frame->size % 4 != 0) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    esp_capture_err_t ret = korvo_read_frame(src, frame);
    if (ret != ESP_CAPTURE_ERR_OK) {
        return ret;
    }
    // Preserve stereo bus timing without averaging cancelling Korvo mic slots.
    uint8_t *pcm = frame->data;
    for (int i = 0; i < frame->size; i += 4) {
        pcm[i + 2] = pcm[i];
        pcm[i + 3] = pcm[i + 1];
    }
    return ESP_CAPTURE_ERR_OK;
}
#endif

static int build_capturer_system(void)
{
    esp_codec_dev_handle_t record_handle = get_record_handle();
    NULL_CHECK(record_handle, "Failed to get record handle");

#if CONFIG_LK_EXAMPLE_ENABLE_AEC
    // Select each codec's measured microphone/reference DMA layout.
    capturer_system.audio_source = livekit_s31_aec_source_new(record_handle,
#if CONFIG_LK_CUSTOM_S31_BOARD_KORVO
        LIVEKIT_S31_AEC_ES8389);
#else
        LIVEKIT_S31_AEC_ES8311);
#endif
    NULL_CHECK(capturer_system.audio_source, "Failed to create AEC source");
#else
    esp_capture_audio_dev_src_cfg_t codec_cfg = {
        .record_handle = record_handle,
    };
    capturer_system.audio_source = esp_capture_new_audio_dev_src(&codec_cfg);
    NULL_CHECK(capturer_system.audio_source, "Failed to create audio source");

#if CONFIG_IDF_TARGET_ESP32S31
    // Both directions share an I2S clock and stereo slots. Convert to the
    // requested Opus format in esp_capture, not by reconfiguring the bus.
    esp_capture_audio_info_t fixed_caps = {
        .format_id = ESP_CAPTURE_FMT_ID_PCM,
        .sample_rate = 48000,
        .channel = 2,
        .bits_per_sample = 16,
    };
    ESP_RETURN_ON_FALSE(capturer_system.audio_source->set_fixed_caps(
                            capturer_system.audio_source, &fixed_caps) == ESP_CAPTURE_ERR_OK,
                        -1, TAG, "Failed to fix S31 capture I2S format");
#if CONFIG_LK_CUSTOM_S31_BOARD_KORVO
    ESP_RETURN_ON_FALSE(esp_codec_dev_set_in_gain(record_handle, 30.0f) == ESP_CODEC_DEV_OK,
                        -1, TAG, "Failed to set Korvo microphone gain");
    korvo_read_frame = capturer_system.audio_source->read_frame;
    capturer_system.audio_source->read_frame = read_korvo_microphone;
#endif
#endif

#endif // CONFIG_LK_EXAMPLE_ENABLE_AEC

    esp_capture_cfg_t cfg = {
        .sync_mode = ESP_CAPTURE_SYNC_MODE_AUDIO,
        .audio_src = capturer_system.audio_source,
    };
    esp_capture_open(&cfg, &capturer_system.capturer_handle);
    NULL_CHECK(capturer_system.capturer_handle, "Failed to open capture system");
    return 0;
}

static int build_renderer_system(void)
{
    esp_codec_dev_handle_t render_device = get_playback_handle();
    NULL_CHECK(render_device, "Failed to get render device handle");

#if CONFIG_IDF_TARGET_ESP32S31
    renderer_system.audio_renderer = livekit_s31_audio_render_alloc(
        render_device, CONFIG_LK_EXAMPLE_SPEAKER_VOLUME);
#else
    i2s_render_cfg_t i2s_cfg = {
        .play_handle = render_device,
    };
    renderer_system.audio_renderer = av_render_alloc_i2s_render(&i2s_cfg);

    esp_codec_dev_set_out_vol(render_device, CONFIG_LK_EXAMPLE_SPEAKER_VOLUME);
#endif
    NULL_CHECK(renderer_system.audio_renderer, "Failed to create I2S renderer");

    av_render_cfg_t render_cfg = {
        .audio_render = renderer_system.audio_renderer,
        .audio_raw_fifo_size = 8 * 4096,
        .audio_render_fifo_size = 100 * 1024,
        .allow_drop_data = false,
    };
    renderer_system.av_renderer_handle = av_render_open(&render_cfg);
    NULL_CHECK(renderer_system.av_renderer_handle, "Failed to create AV renderer");

    av_render_audio_frame_info_t frame_info = {
#if CONFIG_IDF_TARGET_ESP32S31
        .sample_rate = 48000,
        .channel = 2,
#else
        .sample_rate = 16000,
        .channel = get_playback_channels(),
#endif
        .bits_per_sample = 16,
    };
    av_render_set_fixed_frame_info(renderer_system.av_renderer_handle, &frame_info);
    return 0;
}

int media_init(void)
{
    esp_audio_enc_register_default();
    esp_audio_dec_register_default();

    build_capturer_system();
    build_renderer_system();
    return 0;
}

esp_capture_handle_t media_get_capturer(void)
{
    return capturer_system.capturer_handle;
}

av_render_handle_t media_get_renderer(void)
{
    return renderer_system.av_renderer_handle;
}
