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

#include <stdlib.h>
#include <string.h>

#include "esp_aec.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "livekit_s31_aec.h"

// cspell:words AGGR

// Architecture and FIR response follow the owner's OpenWearableAI Korvo
// implementation (phase1/korvo-port, d1a70db). No BLE/LC3 or AFE dependency.
// Read one complete AEC chunk at native bus rate, retaining unused processed
// samples for the next caller. Never change the shared capture/playback clock.
#define AEC_RATE 16000
#define DECIMATION 3
#define FIR_TAPS 33
#define MAX_CHUNK 1024

typedef struct {
    esp_capture_audio_src_if_t base;
    esp_codec_dev_handle_t record;
    aec_handle_t *aec;
    int16_t *native, *mic, *ref, *output;
    int16_t history[2][FIR_TAPS];
    unsigned cursor;
    int chunk, consumed;
    unsigned native_channels, mic_slot, ref_slot;
    uint64_t samples;
    bool opened, started, primed;
} s31_aec_t;

// Hamming-windowed 6.5 kHz low pass at 48 kHz, unity DC gain in Q15.
static const int16_t fir[FIR_TAPS] = {
    45, 12, -52, -126, -132, 18, 301, 512, 352, -302, -1150,
    -1488, -584, 1782, 4989, 7771, 8872, 7771, 4989, 1782,
    -584, -1488, -1150, -302, 352, 512, 301, 18, -132, -126,
    -52, 12, 45,
};

static const esp_capture_audio_info_t output_caps = {
    .format_id = ESP_CAPTURE_FMT_ID_PCM,
    .sample_rate = AEC_RATE, .channel = 1, .bits_per_sample = 16,
};

static void release_processing(s31_aec_t *s)
{
    if (s->aec) {
        aec_destroy(s->aec);
        s->aec = NULL;
    }
    free(s->native); free(s->mic); free(s->ref); free(s->output);
    s->native = s->mic = s->ref = s->output = NULL;
    s->chunk = s->consumed = 0;
}

static esp_capture_err_t source_open(esp_capture_audio_src_if_t *base)
{
    if (!base) return ESP_CAPTURE_ERR_INVALID_ARG;
    ((s31_aec_t *)base)->opened = true;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t source_codecs(esp_capture_audio_src_if_t *base,
                                      const esp_capture_format_id_t **codecs, uint8_t *num)
{
    static const esp_capture_format_id_t pcm = ESP_CAPTURE_FMT_ID_PCM;
    if (!base || !codecs || !num) return ESP_CAPTURE_ERR_INVALID_ARG;
    *codecs = &pcm;
    *num = 1;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t source_fixed(esp_capture_audio_src_if_t *base,
                                     const esp_capture_audio_info_t *caps)
{
    if (!base || !caps) return ESP_CAPTURE_ERR_INVALID_ARG;
    if (((s31_aec_t *)base)->started) return ESP_CAPTURE_ERR_INVALID_STATE;
    return caps->format_id == output_caps.format_id &&
           caps->sample_rate == AEC_RATE && caps->channel == 1 &&
           caps->bits_per_sample == 16 ? ESP_CAPTURE_ERR_OK : ESP_CAPTURE_ERR_NOT_SUPPORTED;
}

static esp_capture_err_t source_negotiate(esp_capture_audio_src_if_t *base,
                                         esp_capture_audio_info_t *in,
                                         esp_capture_audio_info_t *out)
{
    if (!base || !in || !out) return ESP_CAPTURE_ERR_INVALID_ARG;
    if (in->format_id != ESP_CAPTURE_FMT_ID_PCM) return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    *out = output_caps;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t source_start(esp_capture_audio_src_if_t *base)
{
    if (!base) return ESP_CAPTURE_ERR_INVALID_ARG;
    s31_aec_t *s = (s31_aec_t *)base;
    if (!s->opened) return ESP_CAPTURE_ERR_INVALID_STATE;
    if (s->started) return ESP_CAPTURE_ERR_OK;
    aec_config_t cfg = {
        .mic_num = 1, .ref_num = 1, .out_num = 1,
        .filter_length = 4, .sample_rate = AEC_RATE,
        // Keep adaptive state in PSRAM to reserve internal RAM for DMA,
        // aligned PCM buffers and concurrent software video encoding.
        // Both boards passed bounded VOIP overlap tests with this placement;
        // it is a capacity choice, not a measured speed advantage.
        .caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
        .mode = AEC_MODE_VOIP_HIGH_PERF, .nlp_level = AEC_NLP_LEVEL_AGGR,
    };
    s->aec = aec_create_from_config(&cfg);
    if (!s->aec) goto no_memory;
    s->chunk = aec_get_chunksize(s->aec);
    if (s->chunk <= 0 || s->chunk > MAX_CHUNK) {
        release_processing(s);
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    size_t bytes = (size_t)s->chunk * sizeof(int16_t);
    uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    s->native = heap_caps_aligned_alloc(16, bytes * DECIMATION * s->native_channels, caps);
    s->mic = heap_caps_aligned_alloc(16, bytes, caps);
    s->ref = heap_caps_aligned_alloc(16, bytes, caps);
    s->output = heap_caps_aligned_alloc(16, bytes, caps);
    if (!s->native || !s->mic || !s->ref || !s->output) goto no_memory;
    esp_codec_dev_sample_info_t native = {
        .sample_rate = 48000, .channel = s->native_channels, .bits_per_sample = 16,
    };
    if (esp_codec_dev_open(s->record, &native) != ESP_CODEC_DEV_OK) {
        release_processing(s);
        return ESP_CAPTURE_ERR_INTERNAL;
    }
    memset(s->history, 0, sizeof(s->history));
    s->cursor = 0;
    s->samples = 0;
    s->primed = false;
    s->consumed = s->chunk;
    s->started = true;
    ESP_LOGI("s31_aec", "AEC started: VOIP_HIGH_PERF, %d samples, %u native words, mic=%u ref=%u, 48k -> 16k",
             s->chunk, s->native_channels, s->mic_slot, s->ref_slot);
    return ESP_CAPTURE_ERR_OK;
no_memory:
    release_processing(s);
    ESP_LOGE("s31_aec", "AEC allocation failed; raw microphone fallback is disabled");
    return ESP_CAPTURE_ERR_NO_MEM;
}

static void decimate(s31_aec_t *s)
{
    for (int i = 0; i < s->chunk * DECIMATION; ++i) {
        s->history[0][s->cursor] = s->native[s->native_channels * i + s->mic_slot];
        s->history[1][s->cursor] = s->native[s->native_channels * i + s->ref_slot];
        if (i % DECIMATION == 0) {
            int16_t *dest[2] = {s->mic, s->ref};
            for (int ch = 0; ch < 2; ++ch) {
                int64_t sum = 0;
                unsigned pos = s->cursor;
                for (int tap = 0; tap < FIR_TAPS; ++tap) {
                    sum += (int32_t)s->history[ch][pos] * fir[tap];
                    pos = pos ? pos - 1 : FIR_TAPS - 1;
                }
                sum >>= 15;
                dest[ch][i / DECIMATION] = (int16_t)(sum > 32767 ? 32767 :
                                                    sum < -32768 ? -32768 : sum);
            }
        }
        s->cursor = (s->cursor + 1) % FIR_TAPS;
    }
}

static esp_capture_err_t source_read(esp_capture_audio_src_if_t *base,
                                    esp_capture_stream_frame_t *frame)
{
    if (!base || !frame || !frame->data || frame->size <= 0 || frame->size % 2)
        return ESP_CAPTURE_ERR_INVALID_ARG;
    s31_aec_t *s = (s31_aec_t *)base;
    if (!s->started) return ESP_CAPTURE_ERR_INVALID_STATE;
    int remaining = frame->size / 2;
    uint8_t *dest = frame->data;
    frame->pts = (uint32_t)(s->samples * 1000 / AEC_RATE);
    while (remaining) {
        if (s->consumed == s->chunk) {
            if (esp_codec_dev_read(s->record, s->native,
                                  s->chunk * DECIMATION * s->native_channels * sizeof(int16_t)) != ESP_CODEC_DEV_OK)
                return ESP_CAPTURE_ERR_INTERNAL;
            decimate(s);
            // The codec can initially return digital-zero DMA frames while
            // its ADC settles. Offline reproduction with esp-sr 2.5.2
            // (FD_HIGH_PERF) showed permanent saturation after all-zero
            // startup input. Retain this guard with VOIP_HIGH_PERF until the
            // library's startup behavior is established. Preserve those silent
            // frames and their timestamps without advancing adaptive state.
            // After the first signal, process every frame, including silence.
            if (!s->primed) {
                for (int i = 0; i < s->chunk; ++i) {
                    if (s->mic[i] != 0 || s->ref[i] != 0) {
                        s->primed = true;
                        break;
                    }
                }
            }
            if (s->primed) {
                aec_process(s->aec, s->mic, s->ref, s->output);
            } else {
                memset(s->output, 0, (size_t)s->chunk * sizeof(int16_t));
            }
            s->consumed = 0;
        }
        int count = s->chunk - s->consumed;
        if (count > remaining) count = remaining;
        memcpy(dest, s->output + s->consumed, (size_t)count * sizeof(int16_t));
        s->consumed += count;
        s->samples += count;
        remaining -= count;
        dest += count * sizeof(int16_t);
    }
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t source_stop(esp_capture_audio_src_if_t *base)
{
    if (!base) return ESP_CAPTURE_ERR_INVALID_ARG;
    s31_aec_t *s = (s31_aec_t *)base;
    int ret = ESP_CODEC_DEV_OK;
    if (s->started) ret = esp_codec_dev_close(s->record);
    s->started = false;
    release_processing(s);
    return ret == ESP_CODEC_DEV_OK ? ESP_CAPTURE_ERR_OK : ESP_CAPTURE_ERR_INTERNAL;
}

static esp_capture_err_t source_close(esp_capture_audio_src_if_t *base)
{
    esp_capture_err_t ret = source_stop(base);
    if (base) ((s31_aec_t *)base)->opened = false;
    return ret;
}

esp_capture_audio_src_if_t *livekit_s31_aec_source_new(esp_codec_dev_handle_t record,
                                                    livekit_s31_aec_codec_t codec)
{
    if (!record || (codec != LIVEKIT_S31_AEC_ES8311 && codec != LIVEKIT_S31_AEC_ES8389)) return NULL;
    s31_aec_t *s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->record = record;
    // ES8389's reference-enabled stream appends DAC data. Two 16-bit
    // slots capture only microphones; four DMA words expose the references.
    // With 32-bit I2S slots, little-endian DMA places DAC L before mic L.
    s->native_channels = codec == LIVEKIT_S31_AEC_ES8389 ? 4 : 2;
    s->mic_slot = codec == LIVEKIT_S31_AEC_ES8389 ? 1 : 0;
    s->ref_slot = codec == LIVEKIT_S31_AEC_ES8389 ? 0 : 1;
    s->base = (esp_capture_audio_src_if_t) {
        .open = source_open, .get_support_codecs = source_codecs,
        .set_fixed_caps = source_fixed, .negotiate_caps = source_negotiate,
        .start = source_start, .read_frame = source_read,
        .stop = source_stop, .close = source_close,
    };
    return &s->base;
}
