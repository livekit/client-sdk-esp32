/* Media system

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "codec_init.h"
#include "codec_board.h"

#include "esp_capture_path_simple.h"
#include "esp_capture_audio_enc.h"
#include "av_render.h"
#include "common.h"
#include "settings.h"
#include "media_lib_os.h"
#include "esp_timer.h"
#include "av_render_default.h"
#include "esp_audio_dec_default.h"
#include "esp_audio_enc_default.h"
#include "esp_capture_defaults.h"
#include "esp_log.h"

#include "media_setup.h"

#define RET_ON_NULL(ptr, v) do {                                \
    if (ptr == NULL) {                                          \
        ESP_LOGE(TAG, "Memory allocate fail on %d", __LINE__);  \
        return v;                                               \
    }                                                           \
} while (0)

#define TAG "MEDIA_SYS"

typedef struct {
    esp_capture_path_handle_t   capture_handle;
    esp_capture_aenc_if_t      *aud_enc;
    esp_capture_audio_src_if_t *aud_src;
    esp_capture_path_if_t      *path_if;
} capture_system_t;

typedef struct {
    audio_render_handle_t audio_render;
    av_render_handle_t    player;
} player_system_t;

static capture_system_t capture_sys;
static player_system_t  player_sys;

static int build_capture_system(void)
{
    capture_sys.aud_enc = esp_capture_new_audio_encoder();
    RET_ON_NULL(capture_sys.aud_enc, -1);
    // TODO: Enable AEC
    esp_capture_audio_codec_src_cfg_t codec_cfg = {
        .record_handle = get_record_handle()
    };
    capture_sys.aud_src = esp_capture_new_audio_codec_src(&codec_cfg);
    RET_ON_NULL(capture_sys.aud_src, -1);
    esp_capture_simple_path_cfg_t simple_cfg = {
        .aenc = capture_sys.aud_enc,
    };
    capture_sys.path_if = esp_capture_build_simple_path(&simple_cfg);
    RET_ON_NULL(capture_sys.path_if, -1);
    // Create capture system
    esp_capture_cfg_t cfg = {
        .sync_mode = ESP_CAPTURE_SYNC_MODE_AUDIO,
        .audio_src = capture_sys.aud_src,
        .capture_path = capture_sys.path_if,
    };
    esp_capture_open(&cfg, &capture_sys.capture_handle);
    return 0;
}

static int build_player_system()
{
    i2s_render_cfg_t i2s_cfg = {
        .play_handle = get_playback_handle(),
    };
    player_sys.audio_render = av_render_alloc_i2s_render(&i2s_cfg);
    if (player_sys.audio_render == NULL) {
        ESP_LOGE(TAG, "Fail to create audio render");
        return -1;
    }
    esp_codec_dev_set_out_vol(i2s_cfg.play_handle, DEFAULT_PLAYBACK_VOL);
    av_render_cfg_t render_cfg = {
        .audio_render = player_sys.audio_render,
        .audio_raw_fifo_size = 8 * 4096,
        .audio_render_fifo_size = 100 * 1024,
        .allow_drop_data = false,
    };
    player_sys.player = av_render_open(&render_cfg);
    if (player_sys.player == NULL) {
        ESP_LOGE(TAG, "Fail to create player");
        return -1;
    }
    // When support AEC, reference data is from speaker right channel for ES8311 so must output 2 channel
    av_render_audio_frame_info_t aud_info = {
        .sample_rate = 16000,
        .channel = 1,
        .bits_per_sample = 16,
    };
    av_render_set_fixed_frame_info(player_sys.player, &aud_info);
    return 0;
}

int media_setup_init(void)
{
    // Register default audio encoder
    esp_audio_enc_register_default();
    // Register default audio decoder
    esp_audio_dec_register_default();
    // Build capture system
    build_capture_system();
    // Build player system
    build_player_system();
    return 0;
}

esp_capture_handle_t media_setup_get_capturer(void)
{
    return capture_sys.capture_handle;
}

av_render_handle_t media_setup_get_renderer(void)
{
    return player_sys.player;
}