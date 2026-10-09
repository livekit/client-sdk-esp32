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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_codec_dev.h"
int esp_codec_dev_read(esp_codec_dev_handle_t device, void *data, int size);
#include "../../components/s31_board/s31_aec.c"

static int chunk_size = 512, aec_live, codec_live, reads, calls, allocations;
static int fail_alloc, fail_create, fail_open, fail_read;
static int silent_input;
static unsigned native_channels = 2;

void *heap_caps_aligned_alloc(size_t alignment, size_t size, uint32_t caps)
{
    assert(alignment == 16 && caps == (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (++allocations == fail_alloc) return NULL;
    return malloc(size);
}
aec_handle_t *aec_create_from_config(aec_config_t *cfg)
{
    assert(cfg->mic_num == 1 && cfg->ref_num == 1 && cfg->out_num == 1);
    assert(cfg->sample_rate == 16000 && cfg->mode == AEC_MODE_VOIP_HIGH_PERF);
    if (fail_create) return NULL;
    ++aec_live;
    return calloc(1, sizeof(aec_handle_t));
}
int aec_get_chunksize(const aec_handle_t *handle) { assert(handle); return chunk_size; }
void aec_destroy(aec_handle_t *handle) { assert(handle); --aec_live; free(handle); }
void aec_process(const aec_handle_t *handle, int16_t *mic, int16_t *ref, int16_t *out)
{
    assert(handle);
    // Skip startup FIR history, then verify slot isolation and DC gain.
    for (int i = 12; i < chunk_size; ++i) {
        assert(mic[i] == (silent_input ? 0 : 1000));
        assert(ref[i] == (silent_input ? 0 : -2000));
    }
    for (int i = 0; i < chunk_size; ++i) out[i] = (int16_t)((calls * chunk_size + i) % 30000);
    ++calls;
}
int esp_codec_dev_open(esp_codec_dev_handle_t device, esp_codec_dev_sample_info_t *info)
{
    assert(device && info->sample_rate == 48000 && info->channel == native_channels && info->bits_per_sample == 16);
    if (fail_open) return -1;
    ++codec_live;
    return 0;
}
int esp_codec_dev_close(esp_codec_dev_handle_t device) { assert(device); --codec_live; return 0; }
int esp_codec_dev_read(esp_codec_dev_handle_t device, void *data, int size)
{
    assert(device && codec_live == 1 && size == (int)(chunk_size * 3 * native_channels * 2));
    if (fail_read) return -1;
    int16_t *pcm = data;
    for (int i = 0; i < size / 2; i += native_channels) {
        if (native_channels == 2) {pcm[i] = 1000; pcm[i+1] = -2000;}
        else {pcm[i] = -2000; pcm[i+1] = 1000; pcm[i+2] = 1234; pcm[i+3] = -4567;}
    }
    if (silent_input) memset(data, 0, size);
    ++reads;
    return 0;
}

static void test_layout(livekit_s31_aec_codec_t codec)
{
    native_channels = codec == LIVEKIT_S31_AEC_ES8389 ? 4 : 2;
    silent_input = 0;
    assert(!livekit_s31_aec_source_new(NULL, codec));
    esp_capture_audio_src_if_t *s = livekit_s31_aec_source_new((void *)1, codec);
    assert(s && s->start(s) == ESP_CAPTURE_ERR_INVALID_STATE);
    assert(s->open(s) == 0);
    esp_capture_audio_info_t in = {.format_id=ESP_CAPTURE_FMT_ID_PCM,.sample_rate=48000,.channel=2,.bits_per_sample=16}, out;
    assert(s->negotiate_caps(s, &in, &out) == 0 && out.sample_rate == 16000 && out.channel == 1);
    assert(s->set_fixed_caps(s, &in) == ESP_CAPTURE_ERR_NOT_SUPPORTED);
    for (int failure = 1; failure <= 4; ++failure) {
        allocations = 0; fail_alloc = failure;
        assert(s->start(s) == ESP_CAPTURE_ERR_NO_MEM && !aec_live && !codec_live);
    }
    fail_alloc = 0; fail_create = 1;
    assert(s->start(s) == ESP_CAPTURE_ERR_NO_MEM);
    fail_create = 0; chunk_size = MAX_CHUNK + 1;
    assert(s->start(s) == ESP_CAPTURE_ERR_NOT_SUPPORTED && !aec_live);
    chunk_size = 512; fail_open = 1;
    assert(s->start(s) == ESP_CAPTURE_ERR_INTERNAL && !aec_live);
    fail_open = 0;
    for (int cycle = 0; cycle < 10; ++cycle) {
        calls = reads = 0;
        assert(s->start(s) == 0 && s->start(s) == 0 && codec_live == 1);
        int16_t pcm[1000];
        const int sizes[] = {160,320,1,511,1000,48};
        int total = 0;
        for (unsigned j = 0; j < sizeof(sizes)/sizeof(sizes[0]); ++j) {
            esp_capture_stream_frame_t f = {.data=(uint8_t *)pcm,.size=sizes[j]*2};
            assert(s->read_frame(s,&f) == 0 && f.pts == (uint32_t)(total*1000/16000));
            for (int i = 0; i < sizes[j]; ++i) assert(pcm[i] == (total+i)%30000);
            total += sizes[j];
        }
        assert(reads == (total+511)/512);
        esp_capture_stream_frame_t bad = {.data=(uint8_t *)pcm,.size=3};
        assert(s->read_frame(s,&bad) == ESP_CAPTURE_ERR_INVALID_ARG);
        fail_read = 1; bad.size = sizeof(pcm);
        assert(s->read_frame(s,&bad) == ESP_CAPTURE_ERR_INTERNAL);
        fail_read = 0;
        assert(s->stop(s) == 0 && s->stop(s) == 0 && !aec_live && !codec_live);
    }
    for (int cycle = 0; cycle < 2; ++cycle) {
        calls = reads = 0;
        silent_input = 1;
        assert(s->start(s) == 0);
        int16_t pcm[512];
        for (int frame = 0; frame < 2; ++frame) {
            memset(pcm, 0x55, sizeof(pcm));
            esp_capture_stream_frame_t f = {.data=(uint8_t *)pcm,.size=sizeof(pcm)};
            assert(s->read_frame(s, &f) == 0 && f.pts == (uint32_t)(frame * 32));
            assert(calls == 0);
            for (int i = 0; i < 512; ++i) assert(pcm[i] == 0);
        }
        silent_input = 0;
        esp_capture_stream_frame_t f = {.data=(uint8_t *)pcm,.size=sizeof(pcm)};
        assert(s->read_frame(s, &f) == 0 && f.pts == 64 && calls == 1);
        silent_input = 1;
        assert(s->read_frame(s, &f) == 0 && calls == 2);
        assert(s->read_frame(s, &f) == 0 && calls == 3);
        assert(s->stop(s) == 0 && !aec_live && !codec_live);
    }
    assert(s->close(s) == 0 && s->start(s) == ESP_CAPTURE_ERR_INVALID_STATE);
    free(s);
}

int main(void)
{
    assert(!livekit_s31_aec_source_new((void *)1, (livekit_s31_aec_codec_t)99));
    test_layout(LIVEKIT_S31_AEC_ES8311);
    test_layout(LIVEKIT_S31_AEC_ES8389);
    puts("PASS: both codec layouts, caps, FIR, frames, timestamps, allocation failures, restarts, silent startup and continuous post-start AEC");
    return 0;
}
