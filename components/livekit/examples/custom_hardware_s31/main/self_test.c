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

#include "sdkconfig.h"
#include "self_test.h"

#if CONFIG_LK_CUSTOM_S31_SPEAKER_SELF_TEST || CONFIG_LK_CUSTOM_S31_MIC_SELF_TEST

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_codec_dev.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board.h"

#define TEST_SAMPLE_RATE       48000
#define TEST_FRAMES_PER_BLOCK  (TEST_SAMPLE_RATE / 50)
#define TEST_PRIME_BLOCKS      10
#define TEST_TWO_PI            6.28318530717958647692f

static const char *TAG = "custom_s31_self_test";
static int16_t test_pcm[TEST_FRAMES_PER_BLOCK * 2];

#if CONFIG_LK_CUSTOM_S31_MIC_SELF_TEST && CONFIG_LK_CUSTOM_S31_BOARD_FUNCTION_CORE
static int play_mono_cue(int duration_ms)
{
    esp_codec_dev_handle_t playback = get_playback_handle();
    if (playback == NULL) {
        ESP_LOGE(TAG, "Playback device is unavailable");
        return ESP_CODEC_DEV_INVALID_ARG;
    }

    esp_codec_dev_sample_info_t format = {
        .sample_rate = TEST_SAMPLE_RATE,
        .channel = 1,
        .bits_per_sample = 16,
    };
    float phase = 0.0f;
    const float phase_step = TEST_TWO_PI * 180.0f / TEST_SAMPLE_RATE;
    const float amplitude =
        (CONFIG_LK_CUSTOM_S31_SELF_TEST_LEVEL / 100.0f) * INT16_MAX;
    const int block_count = duration_ms / 20;

    int ret = esp_codec_dev_open(playback, &format);
    if (ret != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "Failed to open playback, ret=%d", ret);
        return ret;
    }
    ret = esp_codec_dev_set_out_vol(
        playback, CONFIG_LK_CUSTOM_S31_SELF_TEST_VOLUME);
    if (ret != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "Failed to set playback volume, ret=%d", ret);
        esp_codec_dev_close(playback);
        return ret;
    }

    for (int block = 0; block < block_count; ++block) {
        for (int frame = 0; frame < TEST_FRAMES_PER_BLOCK; ++frame) {
            test_pcm[frame] = (int16_t)(sinf(phase) * amplitude);
            phase += phase_step;
            if (phase >= TEST_TWO_PI) {
                phase -= TEST_TWO_PI;
            }
        }
        ret = esp_codec_dev_write(
            playback, test_pcm, TEST_FRAMES_PER_BLOCK * sizeof(int16_t));
        if (ret != ESP_CODEC_DEV_OK) {
            ESP_LOGE(TAG, "PCM write failed, ret=%d", ret);
            break;
        }
    }

    int close_ret = esp_codec_dev_close(playback);
    return ret == ESP_CODEC_DEV_OK ? close_ret : ret;
}
#endif

#if CONFIG_LK_CUSTOM_S31_SPEAKER_SELF_TEST
#define TEST_BLOCKS_PER_BURST  100
#define TEST_BURST_COUNT       3

static void run_speaker_self_test(void)
{
    esp_codec_dev_handle_t playback = get_playback_handle();
    if (playback == NULL) {
        ESP_LOGE(TAG, "Speaker self-test: playback device is unavailable");
        return;
    }

#if CONFIG_LK_CUSTOM_S31_BOARD_FUNCTION_CORE
    const bool mono_output = true;
#else
    const bool mono_output = false;
#endif
    const int channel_count = mono_output ? 1 : 2;
    const int burst_count = mono_output ? 1 : TEST_BURST_COUNT;
    esp_codec_dev_sample_info_t format = {
        .sample_rate = TEST_SAMPLE_RATE,
        .channel = channel_count,
        .bits_per_sample = 16,
    };
    float phase = 0.0f;
    const float phase_step = TEST_TWO_PI * 180.0f / TEST_SAMPLE_RATE;
    const float amplitude =
        (CONFIG_LK_CUSTOM_S31_SELF_TEST_LEVEL / 100.0f) * INT16_MAX;
    static const char *const channel_names[TEST_BURST_COUNT] = {
        "LEFT", "RIGHT", "BOTH",
    };

    int ret = esp_codec_dev_open(playback, &format);
    if (ret != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "Speaker self-test: failed to open codec, ret=%d", ret);
        return;
    }
    ret = esp_codec_dev_set_out_vol(
        playback, CONFIG_LK_CUSTOM_S31_SELF_TEST_VOLUME);
    if (ret != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "Speaker self-test: failed to set volume, ret=%d", ret);
        esp_codec_dev_close(playback);
        return;
    }

    ESP_LOGI(TAG, "Playing 180 Hz %s test at volume %d%%, PCM level %d%%",
             mono_output ? "mono" : "channel sweep",
             CONFIG_LK_CUSTOM_S31_SELF_TEST_VOLUME,
             CONFIG_LK_CUSTOM_S31_SELF_TEST_LEVEL);
    vTaskDelay(pdMS_TO_TICKS(500));

    // Prime I2S with real zero samples after the PA and codec are enabled.
    memset(test_pcm, 0, sizeof(test_pcm));
    for (int block = 0; block < TEST_PRIME_BLOCKS; ++block) {
        ret = esp_codec_dev_write(
            playback, test_pcm,
            TEST_FRAMES_PER_BLOCK * channel_count * sizeof(int16_t));
        if (ret != ESP_CODEC_DEV_OK) {
            ESP_LOGE(TAG, "Speaker self-test: pre-roll write failed, ret=%d", ret);
            goto done;
        }
    }

    for (int burst = 0; burst < burst_count; ++burst) {
        ESP_LOGI(TAG, "Starting %s burst",
                 mono_output ? "MONO" : channel_names[burst]);
        for (int block = 0; block < TEST_BLOCKS_PER_BURST; ++block) {
            for (int frame = 0; frame < TEST_FRAMES_PER_BLOCK; ++frame) {
                int16_t sample = (int16_t)(sinf(phase) * amplitude);
                if (mono_output) {
                    test_pcm[frame] = sample;
                } else {
                    test_pcm[frame * 2] = burst != 1 ? sample : 0;
                    test_pcm[frame * 2 + 1] = burst != 0 ? sample : 0;
                }
                phase += phase_step;
                if (phase >= TEST_TWO_PI) {
                    phase -= TEST_TWO_PI;
                }
            }
            ret = esp_codec_dev_write(
                playback, test_pcm,
                TEST_FRAMES_PER_BLOCK * channel_count * sizeof(int16_t));
            if (ret != ESP_CODEC_DEV_OK) {
                ESP_LOGE(TAG, "Speaker self-test: PCM write failed, ret=%d", ret);
                goto done;
            }
        }
        if (burst + 1 < burst_count) {
            memset(test_pcm, 0, sizeof(test_pcm));
            for (int pause = 0; pause < 25; ++pause) {
                ret = esp_codec_dev_write(
                    playback, test_pcm,
                    TEST_FRAMES_PER_BLOCK * channel_count * sizeof(int16_t));
                if (ret != ESP_CODEC_DEV_OK) {
                    goto done;
                }
            }
        }
    }

    // Return both channels to digital silence before disabling the PA.
    memset(test_pcm, 0, sizeof(test_pcm));
    for (int block = 0; block < TEST_PRIME_BLOCKS; ++block) {
        ret = esp_codec_dev_write(
            playback, test_pcm,
            TEST_FRAMES_PER_BLOCK * channel_count * sizeof(int16_t));
        if (ret != ESP_CODEC_DEV_OK) {
            ESP_LOGE(TAG, "Speaker self-test: post-roll write failed, ret=%d", ret);
            goto done;
        }
    }

done:
    esp_codec_dev_close(playback);
    ESP_LOGI(TAG, "Speaker self-test finished");
}
#endif

#if CONFIG_LK_CUSTOM_S31_MIC_SELF_TEST
#define MIC_TEST_BLOCK_COUNT 250

static void run_microphone_self_test(void)
{
#if !CONFIG_LK_CUSTOM_S31_BOARD_FUNCTION_CORE
    ESP_LOGW(TAG, "Microphone self-test currently supports only Function-CoreBoard-1");
#else
    ESP_LOGI(TAG, "Microphone self-test start cue");
    if (play_mono_cue(300) != ESP_CODEC_DEV_OK) {
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(250));

    esp_codec_dev_handle_t record = get_record_handle();
    if (record == NULL) {
        ESP_LOGE(TAG, "Microphone self-test: record device is unavailable");
        return;
    }
    esp_codec_dev_sample_info_t format = {
        .sample_rate = TEST_SAMPLE_RATE,
        .channel = 1,
        .bits_per_sample = 16,
    };
    int ret = esp_codec_dev_open(record, &format);
    if (ret != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "Microphone self-test: failed to open capture, ret=%d", ret);
        return;
    }

    uint32_t peak = 0;
    uint64_t square_sum = 0;
    uint32_t sample_count = 0;
    ESP_LOGI(TAG, "Speak or tap near the microphone for five seconds");
    for (int block = 0; block < MIC_TEST_BLOCK_COUNT; ++block) {
        ret = esp_codec_dev_read(
            record, test_pcm, TEST_FRAMES_PER_BLOCK * sizeof(int16_t));
        if (ret != ESP_CODEC_DEV_OK) {
            ESP_LOGE(TAG, "Microphone self-test: PCM read failed, ret=%d", ret);
            break;
        }
        for (int frame = 0; frame < TEST_FRAMES_PER_BLOCK; ++frame) {
            int32_t sample = test_pcm[frame];
            uint32_t magnitude = sample < 0 ? (uint32_t)-sample : (uint32_t)sample;
            if (magnitude > peak) {
                peak = magnitude;
            }
            square_sum += (uint64_t)(sample * sample);
            ++sample_count;
        }
    }
    esp_codec_dev_close(record);

    if (sample_count != 0) {
        double rms = sqrt((double)square_sum / sample_count);
        ESP_LOGI(TAG, "Captured %lu samples, peak=%lu, RMS=%.1f",
                 (unsigned long)sample_count, (unsigned long)peak, rms);
        if (peak < 32) {
            ESP_LOGE(TAG, "Microphone input remained effectively silent");
        }
    }

    ESP_LOGI(TAG, "Microphone self-test stop cue");
    vTaskDelay(pdMS_TO_TICKS(250));
    play_mono_cue(1000);
#endif
}
#endif

#endif

void run_audio_self_tests(void)
{
#if CONFIG_LK_CUSTOM_S31_SPEAKER_SELF_TEST
    run_speaker_self_test();
#endif
#if CONFIG_LK_CUSTOM_S31_MIC_SELF_TEST
    run_microphone_self_test();
#endif
}
