#include "av_render_default.h"
#include "esp_audio_dec_default.h"
#include "esp_audio_enc_default.h"
#include "esp_capture_defaults.h"
#include "esp_capture_sink.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_video_dec_default.h"
#include "esp_video_device.h"
#include "esp_video_enc_default.h"
#include "esp_video_init.h"
#include "codec_board.h"
#include "codec_init.h"

#include "media.h"

static const char *TAG = "media";

#define NULL_CHECK(pointer, message) \
    ESP_RETURN_ON_FALSE(pointer != NULL, -1, TAG, message)

typedef struct {
    esp_capture_sink_handle_t capturer_handle;
    esp_capture_video_src_if_t *video_source;
    esp_capture_audio_src_if_t *audio_source;
} capture_system_t;

typedef struct {
    audio_render_handle_t audio_renderer;
    video_render_handle_t video_renderer;
    av_render_handle_t av_renderer_handle;
} renderer_system_t;

static capture_system_t  capturer_system;
static renderer_system_t renderer_system;

static esp_capture_video_src_if_t* create_camera_source(void)
{
    camera_cfg_t cam_pin_cfg = {};
    int ret = get_camera_cfg(&cam_pin_cfg);
    if (ret != 0) {
        return NULL;
    }
#if CONFIG_IDF_TARGET_ESP32P4
    esp_video_init_csi_config_t csi_config = { 0 };
    esp_video_init_dvp_config_t dvp_config = { 0 };
    esp_video_init_config_t cam_config = { 0 };
    if (cam_pin_cfg.type == CAMERA_TYPE_MIPI) {
        csi_config.sccb_config.i2c_handle = get_i2c_bus_handle(0);
        csi_config.sccb_config.freq = 100000;
        csi_config.reset_pin = cam_pin_cfg.reset;
        csi_config.pwdn_pin = cam_pin_cfg.pwr;
        ESP_LOGI(TAG, "Use i2c handle %p", csi_config.sccb_config.i2c_handle);
        cam_config.csi = &csi_config;
    } else if (cam_pin_cfg.type == CAMERA_TYPE_DVP) {
        dvp_config.reset_pin = cam_pin_cfg.reset;
        dvp_config.pwdn_pin = cam_pin_cfg.pwr;
        dvp_config.dvp_pin.data_width = CAM_CTLR_DATA_WIDTH_8;
        dvp_config.dvp_pin.data_io[0] = cam_pin_cfg.data[0];
        dvp_config.dvp_pin.data_io[1] = cam_pin_cfg.data[1];
        dvp_config.dvp_pin.data_io[2] = cam_pin_cfg.data[2];
        dvp_config.dvp_pin.data_io[3] = cam_pin_cfg.data[3];
        dvp_config.dvp_pin.data_io[4] = cam_pin_cfg.data[4];
        dvp_config.dvp_pin.data_io[5] = cam_pin_cfg.data[5];
        dvp_config.dvp_pin.data_io[6] = cam_pin_cfg.data[6];
        dvp_config.dvp_pin.data_io[7] = cam_pin_cfg.data[7];
        dvp_config.dvp_pin.vsync_io = cam_pin_cfg.vsync;
        dvp_config.dvp_pin.pclk_io = cam_pin_cfg.pclk;
        dvp_config.dvp_pin.xclk_io = cam_pin_cfg.xclk;
        dvp_config.dvp_pin.de_io = cam_pin_cfg.de;
        dvp_config.xclk_freq = 20000000;
        cam_config.dvp = &dvp_config;
    }
    ret = esp_video_init(&cam_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", ret);
        return NULL;
    }
    esp_capture_video_v4l2_src_cfg_t v4l2_cfg = {
        .dev_name = "/dev/video0",
        .buf_count = 2,
    };
    return esp_capture_new_video_v4l2_src(&v4l2_cfg);
#else
    if (cam_pin_cfg.type == CAMERA_TYPE_DVP) {
        esp_capture_video_dvp_src_cfg_t dvp_config = { 0 };
        dvp_config.buf_count = 2;
        dvp_config.reset_pin = cam_pin_cfg.reset;
        dvp_config.pwr_pin = cam_pin_cfg.pwr;
        dvp_config.data[0] = cam_pin_cfg.data[0];
        dvp_config.data[1] = cam_pin_cfg.data[1];
        dvp_config.data[2] = cam_pin_cfg.data[2];
        dvp_config.data[3] = cam_pin_cfg.data[3];
        dvp_config.data[4] = cam_pin_cfg.data[4];
        dvp_config.data[5] = cam_pin_cfg.data[5];
        dvp_config.data[6] = cam_pin_cfg.data[6];
        dvp_config.data[7] = cam_pin_cfg.data[7];
        dvp_config.vsync_pin = cam_pin_cfg.vsync;
        dvp_config.href_pin = cam_pin_cfg.href;
        dvp_config.pclk_pin = cam_pin_cfg.pclk;
        dvp_config.xclk_pin = cam_pin_cfg.xclk;
        dvp_config.xclk_freq = 20000000;
        return esp_capture_new_video_dvp_src(&dvp_config);
    }
#endif
}

static int build_capturer_system(void)
{
    capturer_system.video_source = create_camera_source();
    NULL_CHECK(capturer_system.video_source, "Failed to create camera source");

    esp_codec_dev_handle_t record_handle = get_record_handle();
    NULL_CHECK(record_handle, "Failed to get record handle");

    // For supported boards, prefer using an acoustic echo cancellation (AEC) source
    // for applications requiring hands-free voice communication:
    //
    // esp_capture_audio_aec_src_cfg_t codec_cfg = {
    //     .record_handle = record_handle,
    //     .channel = 4,
    //     .channel_mask = 1 | 2
    // };
    // capturer_system.audio_source = esp_capture_new_audio_aec_src(&codec_cfg);

    esp_capture_audio_dev_src_cfg_t codec_cfg = {
        .record_handle = record_handle,
    };
    capturer_system.audio_source = esp_capture_new_audio_dev_src(&codec_cfg);

    NULL_CHECK(capturer_system.audio_source, "Failed to create audio source");

    esp_capture_cfg_t cfg = {
        .sync_mode = ESP_CAPTURE_SYNC_MODE_AUDIO,
        .audio_src = capturer_system.audio_source,
        .video_src = capturer_system.video_source
    };
    esp_capture_open(&cfg, &capturer_system.capturer_handle);
    NULL_CHECK(capturer_system.capturer_handle, "Failed to open capture system");
    return 0;
}

static int build_renderer_system(void)
{
    esp_codec_dev_handle_t render_device = get_playback_handle();
    NULL_CHECK(render_device, "Failed to get render device handle");

    i2s_render_cfg_t i2s_cfg = {
        .play_handle = render_device,
        .fixed_clock = true
    };
    renderer_system.audio_renderer = av_render_alloc_i2s_render(&i2s_cfg);
    NULL_CHECK(renderer_system.audio_renderer, "Failed to create I2S renderer");

    // Set initial speaker volume
    esp_codec_dev_set_out_vol(i2s_cfg.play_handle, CONFIG_LK_EXAMPLE_SPEAKER_VOLUME);

    av_render_cfg_t render_cfg = {
        .audio_render = renderer_system.audio_renderer,
        .audio_raw_fifo_size = 8 * 4096,
        .audio_render_fifo_size = 100 * 1024,
        .video_raw_fifo_size = 500 * 1024,
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

    // Register default video encoder and decoder
    esp_video_enc_register_default();
    esp_video_dec_register_default();

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