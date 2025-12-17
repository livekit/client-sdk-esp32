#include "sdkconfig.h"
#include "esp_check.h"
#include "esp_log.h"
#include "av_render_default.h"
#include "esp_audio_dec_default.h"
#include "esp_audio_enc_default.h"
#include "esp_capture_defaults.h"
#include "esp_capture_sink.h"

#include "board.h"
#include "media.h"

static const char *TAG = "media";

// Some IDE/lint setups (or stale build dirs) don't automatically include regenerated sdkconfig entries.
// Keep a safe default so builds remain reproducible without a full reconfigure.
#ifndef CONFIG_LK_EXAMPLE_AEC_MIC_LAYOUT
#define CONFIG_LK_EXAMPLE_AEC_MIC_LAYOUT "MMRN"
#endif

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

static capture_system_t  capturer_system;
static renderer_system_t renderer_system;

static int media_i2s_render_ref_cb(uint8_t *data, int size, void *ctx)
{
    (void)ctx;
    if (data == NULL || size <= 0) return 0;

    // `av_render` provides decoded PCM frames to the I2S renderer.
    // Compute a simple peak level and push it to the UI visualizer.
    const int16_t *samples = (const int16_t *)data;
    const int sample_count = size / (int)sizeof(int16_t);

    int32_t peak = 0;
    for (int i = 0; i < sample_count; i++) {
        int32_t v = samples[i];
        if (v < 0) v = -v;
        if (v > peak) peak = v;
    }
    float level = (float)peak / 32768.0f;
    board_visualizer_set_level(level);
    return 0;
}

static int build_capturer_system(void)
{
    esp_codec_dev_handle_t record_handle = (esp_codec_dev_handle_t)board_get_mic_handle();
    NULL_CHECK(record_handle, "Failed to get record handle");

    // Enable AEC on the capture input (Option A / ES7210 TDM):
    // - ch0: Mic1 (near-end mic)
    // - ch1: Mic2 (near-end mic)
    // - ch2: Mic3 (AEC reference input)
    // - ch3: unused
    //
    // IMPORTANT: do NOT set channel_mask here. We need to feed mic+ref into the AFE.
    // The output published to LiveKit remains mono (AEC-processed).
    esp_capture_audio_aec_src_cfg_t codec_cfg = {
        .record_handle = record_handle,
        .channel = 4,
        .channel_mask = 1 | 2
    };
    capturer_system.audio_source = esp_capture_new_audio_aec_src(&codec_cfg);
    NULL_CHECK(capturer_system.audio_source, "Failed to create audio source");

    esp_capture_cfg_t cfg = {
        .sync_mode = ESP_CAPTURE_SYNC_MODE_AUDIO,
        .audio_src = capturer_system.audio_source
    };
    esp_capture_open(&cfg, &capturer_system.capturer_handle);
    NULL_CHECK(capturer_system.capturer_handle, "Failed to open capture system");
    return 0;
}

static int build_renderer_system(void)
{
    esp_codec_dev_handle_t render_device = (esp_codec_dev_handle_t)board_get_speaker_handle();
    NULL_CHECK(render_device, "Failed to get render device handle");

    i2s_render_cfg_t i2s_cfg = {
        .play_handle = render_device,
        .cb = media_i2s_render_ref_cb,
        .fixed_clock = true,
        .ctx = NULL,
    };
    renderer_system.audio_renderer = av_render_alloc_i2s_render(&i2s_cfg);
    NULL_CHECK(renderer_system.audio_renderer, "Failed to create I2S renderer");

    // Set initial speaker volume
    esp_codec_dev_set_out_vol(i2s_cfg.play_handle, CONFIG_LK_EXAMPLE_SPEAKER_VOLUME);

    av_render_cfg_t render_cfg = {
        .audio_render = renderer_system.audio_renderer,
        .audio_raw_fifo_size = 8 * 4096,
        .audio_render_fifo_size = 100 * 1024,
        .allow_drop_data = false,
    };
    renderer_system.av_renderer_handle = av_render_open(&render_cfg);
    NULL_CHECK(renderer_system.av_renderer_handle, "Failed to create AV renderer");

    av_render_audio_frame_info_t frame_info = {
        .sample_rate = 16000,
        .channel = 2,
        .bits_per_sample = 16,
    };
    av_render_set_fixed_frame_info(renderer_system.av_renderer_handle, &frame_info);

    return 0;
}

int media_init(void)
{
    // Register default audio encoder and decoder
    esp_audio_enc_register_default();
    esp_audio_dec_register_default();

    // Build capturer and renderer systems
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