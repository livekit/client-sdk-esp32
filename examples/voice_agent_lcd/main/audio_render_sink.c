#include "av_render.h"
#include "esp_capture.h"

#include "av_render_default.h"
#include "codec_init.h"
#include "esp_audio_dec_default.h"
#include "esp_audio_enc_default.h"
#include "esp_capture_audio_enc.h"
#include "esp_capture_defaults.h"
#include "esp_capture_path_simple.h"
#include "esp_check.h"
#include "esp_log.h"
#include "media_lib_os.h"

#include "audio_render_sink.h"
#include "fft.h"
#include "media.h"

#include "audio_visualizer.h"

audio_render_handle_t real_render = NULL;
static const char *TAG = "au_render_sink";


typedef struct {
  audio_render_handle_t audio_renderer;
  av_render_handle_t av_renderer_handle;
} renderer_system_t;

static renderer_system_t renderer_system;

av_render_audio_frame_info_t frame_info = {
    .sample_rate = 16000,
    .channel = 2,
    .bits_per_sample = 16,
};

static audio_render_handle_t au_render_sink_init(void *cfg, int cfg_size) {
  if (cfg_size != sizeof(i2s_render_cfg_t)) {
    return NULL;
  }

  if (real_render == NULL) {
    real_render = av_render_alloc_i2s_render((i2s_render_cfg_t *)cfg);
  }

  audio_visualizer_init();

  return (audio_render_handle_t)real_render;
}

static int au_render_sink_open(audio_render_handle_t render,
                               av_render_audio_frame_info_t *info) {
  int ret = 0;
  if (real_render != NULL) {
    ret = audio_render_open(real_render, info);
  }
  return ret;
}

static int au_render_sink_write(audio_render_handle_t render,
                                av_render_audio_frame_t *audio_data) {
  if (real_render) {
    //ESP_LOGE(TAG, "Write audio data: pts=%lu, size=%d", audio_data->pts,
    //         audio_data->size);
    audio_visualizer_processing(audio_data->data, audio_data->size);
    // Write audio data to the render
    audio_render_write(real_render, audio_data);
  }
  return 0;
}

static int au_render_sink_get_latency(audio_render_handle_t render,
                                      uint32_t *latency) {
  return audio_render_get_latency(real_render, latency);
}

static int au_render_sink_get_frame_info(audio_render_handle_t render,
                                         av_render_audio_frame_info_t *info) {
  return audio_render_get_frame_info(real_render, info);
}

static int au_render_sink_set_speed(audio_render_handle_t render, float speed) {
  return audio_render_set_speed(real_render, speed);
}

static int au_render_sink_close(audio_render_handle_t render) {
  int ret = 0;
  if (real_render != NULL) {
    ret = audio_render_close(real_render);
    if (ret != 0) {
      ESP_LOGE(TAG, "Failed to close render: %d", ret);
    }
    real_render = NULL;

    audio_visualizer_deinit();
    ESP_LOGI(TAG, "Audio render sink closed");
  }
  return ret;
}

int media_sys_set_audio_focus(int src) { return 0; }

static audio_render_handle_t
av_render_alloc_au_render_sink(i2s_render_cfg_t *i2s_cfg) {
  audio_render_cfg_t cfg = {
      .ops =
          {
              .init = au_render_sink_init,
              .open = au_render_sink_open,
              .write = au_render_sink_write,
              .get_latency = au_render_sink_get_latency,
              .set_speed = au_render_sink_set_speed,
              .get_frame_info = au_render_sink_get_frame_info,
              .close = au_render_sink_close,
          },
      .cfg = i2s_cfg,
      .cfg_size = sizeof(i2s_render_cfg_t),
  };
  return audio_render_alloc_handle(&cfg);
}

int build_player_with_sink_system() {
  i2s_render_cfg_t i2s_cfg = {
      .play_handle = get_playback_handle(),
  };
  renderer_system.audio_renderer = av_render_alloc_au_render_sink(&i2s_cfg);
  if (renderer_system.audio_renderer == NULL) {
    ESP_LOGE(TAG, "Fail to create audio render");
    return -1;
  }
  esp_codec_dev_set_out_vol(i2s_cfg.play_handle, CONFIG_DEFAULT_PLAYBACK_VOL);

  av_render_cfg_t render_cfg = {
      .audio_render = renderer_system.audio_renderer,
      .audio_raw_fifo_size = 8 * 4096,
      .audio_render_fifo_size = 100 * 1024,
      .allow_drop_data = false,
  };

  renderer_system.av_renderer_handle = av_render_open(&render_cfg);

  if (renderer_system.av_renderer_handle == NULL) {
    ESP_LOGE(TAG, "Fail to create player");
    return -1;
  }
  // When support AEC, reference data is from speaker right channel for ES8311
  // so must output 2 channel
  av_render_set_fixed_frame_info(renderer_system.av_renderer_handle,
                                 &frame_info);
  return 0;
}

av_render_handle_t media_get_renderer(void) {
  return renderer_system.av_renderer_handle;
}