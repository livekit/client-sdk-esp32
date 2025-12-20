#include "sdkconfig.h"
#include "esp_check.h"
#include "esp_log.h"
#include "av_render_default.h"
#include "esp_audio_dec_default.h"
#include "esp_audio_enc_default.h"
#include "esp_capture_defaults.h"
#include "esp_capture_sink.h"

#include <string.h>
#include <stdlib.h>

#include "board.h"
#include "media.h"

static const char *TAG = "media";

static volatile bool s_mic_muted = false;

// Some IDE/lint setups (or stale build dirs) don't automatically include regenerated sdkconfig entries.
// Keep a safe default so builds remain reproducible without a full reconfigure.
#ifndef CONFIG_LK_EXAMPLE_AEC_MIC_LAYOUT
#define CONFIG_LK_EXAMPLE_AEC_MIC_LAYOUT "MMRN"
#endif

#define NULL_CHECK(pointer, message) \
    ESP_RETURN_ON_FALSE(pointer != NULL, -1, TAG, message)

// Post-AEC digital gain (hard-coded).
//
// The ESP-SR AEC/AFE can reduce perceived level; this stage boosts the AEC output
// *before* it is encoded and published.
//
// NOTE: This is intentionally hard-coded (no sdkconfig knobs). Tweak if needed.
//  - 5/2 = +7.96 dB
//  - 2/1 = +6.02 dB
//  - 3/1 = +9.54 dB (more clipping risk)
#define LK_POST_AEC_GAIN_NUM 2
#define LK_POST_AEC_GAIN_DEN 1

typedef struct {
    esp_capture_audio_src_if_t  iface;  // must be first
    esp_capture_audio_src_if_t *inner;
} lk_post_gain_audio_src_t;

static inline int16_t lk_clip_i16(int32_t v)
{
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
}

static esp_capture_err_t lk_pg_open(esp_capture_audio_src_if_t *src)
{
    lk_post_gain_audio_src_t *s = (lk_post_gain_audio_src_t *)src;
    return s->inner->open(s->inner);
}

static esp_capture_err_t lk_pg_get_support_codecs(esp_capture_audio_src_if_t *src, const esp_capture_format_id_t **codecs, uint8_t *num)
{
    lk_post_gain_audio_src_t *s = (lk_post_gain_audio_src_t *)src;
    return s->inner->get_support_codecs(s->inner, codecs, num);
}

static esp_capture_err_t lk_pg_set_fixed_caps(esp_capture_audio_src_if_t *src, const esp_capture_audio_info_t *fixed_caps)
{
    lk_post_gain_audio_src_t *s = (lk_post_gain_audio_src_t *)src;
    return s->inner->set_fixed_caps(s->inner, fixed_caps);
}

static esp_capture_err_t lk_pg_negotiate_caps(esp_capture_audio_src_if_t *src, esp_capture_audio_info_t *in_caps, esp_capture_audio_info_t *out_caps)
{
    lk_post_gain_audio_src_t *s = (lk_post_gain_audio_src_t *)src;
    return s->inner->negotiate_caps(s->inner, in_caps, out_caps);
}

static esp_capture_err_t lk_pg_start(esp_capture_audio_src_if_t *src)
{
    lk_post_gain_audio_src_t *s = (lk_post_gain_audio_src_t *)src;
    return s->inner->start(s->inner);
}

static esp_capture_err_t lk_pg_read_frame(esp_capture_audio_src_if_t *src, esp_capture_stream_frame_t *frame)
{
    lk_post_gain_audio_src_t *s = (lk_post_gain_audio_src_t *)src;
    esp_capture_err_t err = s->inner->read_frame(s->inner, frame);
    if (err != ESP_CAPTURE_ERR_OK || frame == NULL || frame->data == NULL || frame->size <= 0) {
        return err;
    }

    // IMPORTANT: `s->inner` is the ESP Capture AEC source, so `frame->data` here is the
    // post-AEC (AFE output) mono PCM16 that we publish to LiveKit.
    //
    // Apply post-AEC gain in-place with saturation.
    if ((frame->size % (int)sizeof(int16_t)) != 0) {
        return err;
    }

    int16_t *pcm = (int16_t *)frame->data;
    const int n = frame->size / (int)sizeof(int16_t);
    for (int i = 0; i < n; i++) {
        const int32_t v = ((int32_t)pcm[i] * (int32_t)LK_POST_AEC_GAIN_NUM) / (int32_t)LK_POST_AEC_GAIN_DEN;
        pcm[i] = lk_clip_i16(v);
    }

    // Software mic mute: publish silence while keeping timing/track alive.
    if (s_mic_muted) {
        memset(frame->data, 0, (size_t)frame->size);
    }

    // Mic input level visualizer:
    // compute from the exact buffer we publish (post-AEC, post-gain, post-mute),
    // not from raw codec mic samples.
    int32_t peak = 0;
    for (int i = 0; i < n; i++) {
        int32_t v = pcm[i];
        if (v < 0) v = -v;
        if (v > peak) peak = v;
    }
    const float level = (float)peak / 32768.0f;
    board_mic_visualizer_set_level(level);
    return err;
}

static esp_capture_err_t lk_pg_stop(esp_capture_audio_src_if_t *src)
{
    lk_post_gain_audio_src_t *s = (lk_post_gain_audio_src_t *)src;
    return s->inner->stop(s->inner);
}

static esp_capture_err_t lk_pg_close(esp_capture_audio_src_if_t *src)
{
    lk_post_gain_audio_src_t *s = (lk_post_gain_audio_src_t *)src;
    esp_capture_err_t err = s->inner->close(s->inner);
    free(s);
    return err;
}

static esp_capture_audio_src_if_t *lk_wrap_post_aec_gain(esp_capture_audio_src_if_t *inner)
{
    if (inner == NULL) return NULL;

    lk_post_gain_audio_src_t *s = (lk_post_gain_audio_src_t *)calloc(1, sizeof(lk_post_gain_audio_src_t));
    if (s == NULL) {
        return inner; // best-effort fallback: no post gain
    }
    s->inner = inner;
    s->iface.open = lk_pg_open;
    s->iface.get_support_codecs = lk_pg_get_support_codecs;
    s->iface.set_fixed_caps = lk_pg_set_fixed_caps;
    s->iface.negotiate_caps = lk_pg_negotiate_caps;
    s->iface.start = lk_pg_start;
    s->iface.read_frame = lk_pg_read_frame;
    s->iface.stop = lk_pg_stop;
    s->iface.close = lk_pg_close;
    return &s->iface;
}

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
    // The output published to LiveKit remains mono (AEC-processed).
    esp_capture_audio_aec_src_cfg_t codec_cfg = {
        .record_handle = record_handle,
        .channel = 4,
        .channel_mask = 1 | 2
    };
    capturer_system.audio_source = esp_capture_new_audio_aec_src(&codec_cfg);
    capturer_system.audio_source = lk_wrap_post_aec_gain(capturer_system.audio_source);
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

void media_set_mic_muted(bool muted)
{
    s_mic_muted = muted;
}

bool media_get_mic_muted(void)
{
    return s_mic_muted;
}

bool media_toggle_mic_muted(void)
{
    s_mic_muted = !s_mic_muted;
    return s_mic_muted;
}