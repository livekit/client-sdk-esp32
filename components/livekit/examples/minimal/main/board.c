#include "esp_log.h"
#include "esp_check.h"

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "lvgl.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "board.h"
#include "media.h"

static const char *TAG = "board";

// Some IDE/lint setups don't automatically include the generated `sdkconfig.h`.
// Keep this as a harmless fallback to avoid false-positive diagnostics.
#ifndef CONFIG_LK_EXAMPLE_CODEC_BOARD_TYPE
#define CONFIG_LK_EXAMPLE_CODEC_BOARD_TYPE "UNUSED"
#endif

// This example uses the Waveshare ESP32-S3-Touch-AMOLED-2.06 BSP to initialize audio I/O.
static esp_codec_dev_handle_t s_mic_handle = NULL;
static esp_codec_dev_handle_t s_spk_handle = NULL;

// UI visualizer state.
static lv_obj_t *s_visualizer_bar = NULL;
static lv_obj_t *s_visualizer_dot = NULL;
static lv_timer_t *s_visualizer_timer = NULL;
static volatile uint16_t s_visualizer_level_q15 = 0; // updated from audio thread
static uint16_t s_visualizer_display_q15 = 0;        // smoothed display value

// Mic mute UI indicator (top-right).
static lv_obj_t *s_mic_mute_dot = NULL;

// Upper button → mute toggle task plumbing.
typedef struct {
    bsp_button_t button;
} board_button_evt_t;
static QueueHandle_t s_button_queue = NULL;
static TaskHandle_t s_button_task = NULL;

static void board_mic_mute_dot_apply(bool muted)
{
    if (s_mic_mute_dot == NULL) return;
    const lv_color_t c = muted ? lv_color_hex(0xFF0000) : lv_color_hex(0x0000FF);
    lv_obj_set_style_bg_color(s_mic_mute_dot, c, 0);
}

static void board_visualizer_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (s_visualizer_bar == NULL) return;

    const uint16_t target = s_visualizer_level_q15;
    uint16_t cur = s_visualizer_display_q15;

    // Simple peak-hold + exponential decay for a pleasant “meter” feel.
    if (target > cur) {
        cur = target;
    } else {
        // decay ~10% per tick (~33ms default) => fast falloff
        cur = (uint16_t)((uint32_t)cur * 9U / 10U);
    }
    s_visualizer_display_q15 = cur;

    const int32_t v = (int32_t)((uint32_t)cur * 100U / 32767U); // 0..100

    // Switch to the silent dot as soon as the bar would be smaller than the dot.
    // This avoids a visible “jump” between a tiny bar and the (20px) dot.
    const int32_t bar_w = lv_obj_get_width(s_visualizer_bar);
    const int32_t dot_d = s_visualizer_dot ? lv_obj_get_width(s_visualizer_dot) : 20;
    // In RANGE mode with [-100..100] and start=-v/value=+v, the total filled width is v% of the bar.
    const int32_t fill_px = (bar_w > 0) ? (int32_t)(((int64_t)bar_w * (int64_t)v) / 100) : 0;
    const bool show_dot = (v == 0) || (fill_px < dot_d);

    if (show_dot) {
        lv_bar_set_start_value(s_visualizer_bar, 0, LV_ANIM_OFF);
        lv_bar_set_value(s_visualizer_bar, 0, LV_ANIM_OFF);
        lv_obj_add_flag(s_visualizer_bar, LV_OBJ_FLAG_HIDDEN);
        if (s_visualizer_dot) lv_obj_clear_flag(s_visualizer_dot, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_clear_flag(s_visualizer_bar, LV_OBJ_FLAG_HIDDEN);
    if (s_visualizer_dot) lv_obj_add_flag(s_visualizer_dot, LV_OBJ_FLAG_HIDDEN);

    // Center-out meter: fill from -v..+v.
    // LVGL v9 bar supports RANGE mode (start_value..value).
    lv_bar_set_start_value(s_visualizer_bar, -v, LV_ANIM_OFF);
    lv_bar_set_value(s_visualizer_bar, v, LV_ANIM_OFF);
}

// Embedded boot image.
// Provided by `EMBED_FILES "boot.png"` in `main/CMakeLists.txt`.
extern const uint8_t boot_png_start[] asm("_binary_boot_png_start");
extern const uint8_t boot_png_end[]   asm("_binary_boot_png_end");

static const lv_image_dsc_t *board_boot_png_dsc(void)
{
    // IMPORTANT: LVGL keeps a pointer to the `lv_image_dsc_t` provided via `lv_image_set_src`.
    // So it must live for the lifetime of the image object (not on the stack).
    static lv_image_dsc_t s_dsc;
    static bool s_inited = false;
    if (!s_inited) {
        const uint8_t *start = &boot_png_start[0];
        const uint8_t *end = &boot_png_end[0];
        const size_t sz = (size_t)(end - start);
        s_dsc = (lv_image_dsc_t){
            .header.magic = LV_IMAGE_HEADER_MAGIC,
            .header.cf = LV_COLOR_FORMAT_RAW, // PNG bytes; decoded by LVGL lodepng
            .header.flags = 0,
            .header.w = 0,
            .header.h = 0,
            .header.stride = 0,
            .data_size = (uint32_t)sz,
            .data = start,
            .reserved = NULL,
            .reserved_2 = NULL,
        };
        s_inited = true;
    }
    return &s_dsc;
}

static bool board_png_get_wh(const uint8_t *png, size_t png_len, uint32_t *w, uint32_t *h)
{
    // PNG signature (8 bytes) + IHDR chunk:
    // - width @ offset 16..19 (big-endian)
    // - height @ offset 20..23 (big-endian)
    static const uint8_t magic[] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
    if (png == NULL || png_len < 24) return false;
    if (memcmp(png, magic, sizeof(magic)) != 0) return false;
    uint32_t be_w = ((uint32_t)png[16] << 24) | ((uint32_t)png[17] << 16) | ((uint32_t)png[18] << 8) | (uint32_t)png[19];
    uint32_t be_h = ((uint32_t)png[20] << 24) | ((uint32_t)png[21] << 16) | ((uint32_t)png[22] << 8) | (uint32_t)png[23];
    if (be_w == 0 || be_h == 0) return false;
    *w = be_w;
    *h = be_h;
    return true;
}

static void board_display_init_and_show_image(void)
{
    lv_display_t *disp = bsp_display_start();
    if (disp == NULL) {
        ESP_LOGE(TAG, "Failed to start BSP display");
        return;
    }

    // Optional: set brightness (0-100). bsp_display_start() already initializes brightness.
    bsp_display_brightness_set(80);

    // LVGL is not thread-safe: always take the BSP LVGL lock before calling LVGL APIs.
    bsp_display_lock(0);

    const uint8_t *png_start = &boot_png_start[0];
    const uint8_t *png_end = &boot_png_end[0];
    if (png_end <= png_start) {
        ESP_LOGE(TAG, "boot.png not embedded or invalid (did you set EMBED_FILES \"boot.png\"?)");
        bsp_display_unlock();
        return;
    }

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // Diagnostics: confirm decoder + PNG size.
    uint32_t png_w = 0, png_h = 0;
    const size_t png_len = (size_t)(png_end - png_start);
    const bool wh_ok = board_png_get_wh(png_start, png_len, &png_w, &png_h);
    if (!wh_ok) {
        ESP_LOGW(TAG, "boot.png: failed to parse width/height (len=%u)", (unsigned)png_len);
    } else {
        // LVGL lodepng decodes to ARGB8888: 4 bytes/pixel.
        const uint64_t decoded_bytes = (uint64_t)png_w * (uint64_t)png_h * 4ULL;
        ESP_LOGI(TAG, "boot.png: %ux%u, embedded=%u bytes, decoded≈%u KB",
                 (unsigned)png_w, (unsigned)png_h, (unsigned)png_len, (unsigned)(decoded_bytes / 1024ULL));
    }

    // Centered column layout: logo + visualizer bar underneath.
    lv_obj_t *cont = lv_obj_create(scr);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cont, 12, 0);

    lv_obj_t *img = lv_image_create(cont);
    lv_image_set_src(img, board_boot_png_dsc());

    // Visualizer: a center-out meter looks nicer than a left-to-right fill.
    // We render a RANGE-mode bar and drive it with (-v..+v).
    // When silent, we hide the bar and show a small neutral ring instead.
    lv_obj_t *viz = lv_obj_create(cont);
    lv_obj_remove_style_all(viz);
    lv_obj_set_size(viz, 220, 20);

    s_visualizer_bar = lv_bar_create(viz);
    lv_obj_set_size(s_visualizer_bar, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_radius(s_visualizer_bar, 10, 0);
    // Black track so it blends into the screen background.
    lv_obj_set_style_bg_color(s_visualizer_bar, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_visualizer_bar, LV_OPA_COVER, 0);

    // Indicator gradient (left -> right), matching the reference image.
    lv_obj_set_style_bg_color(s_visualizer_bar, lv_color_hex(0xD86AAE), LV_PART_INDICATOR);      // pink-ish
    lv_obj_set_style_bg_grad_color(s_visualizer_bar, lv_color_hex(0x7B86FF), LV_PART_INDICATOR); // blue-ish
    lv_obj_set_style_bg_grad_dir(s_visualizer_bar, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_visualizer_bar, LV_OPA_COVER, LV_PART_INDICATOR);

    // Center at 0; fill between start_value..value.
    lv_bar_set_mode(s_visualizer_bar, LV_BAR_MODE_RANGE);
    lv_bar_set_range(s_visualizer_bar, -100, 100);
    lv_bar_set_start_value(s_visualizer_bar, 0, LV_ANIM_OFF);
    lv_bar_set_value(s_visualizer_bar, 0, LV_ANIM_OFF);

    // Silent-state dot: filled with the midpoint purple of the gradient so the
    // transition to the bar looks seamless. Match diameter to bar height (20px).
    s_visualizer_dot = lv_obj_create(viz);
    lv_obj_remove_style_all(s_visualizer_dot);
    lv_obj_set_size(s_visualizer_dot, 20, 20);
    lv_obj_set_style_radius(s_visualizer_dot, LV_RADIUS_CIRCLE, 0);
    // Midpoint between 0xD86AAE and 0x7B86FF (approx).
    lv_obj_set_style_bg_color(s_visualizer_dot, lv_color_hex(0xA978D6), 0);
    lv_obj_set_style_bg_opa(s_visualizer_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_visualizer_dot, 0, 0);
    lv_obj_align(s_visualizer_dot, LV_ALIGN_CENTER, 0, 0);

    // Start silent (ring visible).
    lv_obj_add_flag(s_visualizer_bar, LV_OBJ_FLAG_HIDDEN);

    // Mic mute indicator: always visible, top-right.
    // Create after the rest of the UI and force it to the foreground so it can't be obscured.
    // - Blue: unmuted
    // - Red: muted
    s_mic_mute_dot = lv_obj_create(scr);
    lv_obj_remove_style_all(s_mic_mute_dot);
    lv_obj_set_size(s_mic_mute_dot, 16, 16);
    lv_obj_set_style_radius(s_mic_mute_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_mic_mute_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_mic_mute_dot, 2, 0);
    lv_obj_set_style_border_color(s_mic_mute_dot, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_shadow_width(s_mic_mute_dot, 0, 0);
    lv_obj_clear_flag(s_mic_mute_dot, LV_OBJ_FLAG_CLICKABLE);
    // Top-center (rounded corners can clip top-right).
    lv_obj_align(s_mic_mute_dot, LV_ALIGN_TOP_MID, 0, 10);
    board_mic_mute_dot_apply(media_get_mic_muted());
    lv_obj_move_foreground(s_mic_mute_dot);

#if !LV_USE_LODEPNG
    ESP_LOGW(TAG, "PNG decoder disabled: enable CONFIG_LV_USE_LODEPNG");
    bsp_display_unlock();
    return;
#else
    (void)wh_ok;
#endif

    // Timer-driven animation (runs in LVGL task context).
    if (s_visualizer_timer == NULL) {
        s_visualizer_timer = lv_timer_create(board_visualizer_timer_cb, 33, NULL);
    }

    bsp_display_unlock();
}

void board_set_mic_muted(bool muted)
{
    // Safe to call from non-LVGL contexts.
    if (!bsp_display_lock(0)) {
        return;
    }
    board_mic_mute_dot_apply(muted);
    bsp_display_unlock();
}

void *board_get_mic_handle(void)
{
    return s_mic_handle;
}

void *board_get_speaker_handle(void)
{
    return s_spk_handle;
}

static void IRAM_ATTR board_button_isr_cb(bsp_button_t button, void *ctx)
{
    (void)ctx;
    if (s_button_queue == NULL) return;
    const board_button_evt_t evt = {.button = button};
    BaseType_t hp_task_woken = pdFALSE;
    (void)xQueueSendFromISR(s_button_queue, &evt, &hp_task_woken);
    if (hp_task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void board_button_task_fn(void *arg)
{
    (void)arg;
    board_button_evt_t evt;
    bool upper_down = false;
    while (true) {
        if (xQueueReceive(s_button_queue, &evt, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (evt.button != BSP_BUTTON_UPPER) {
            continue;
        }
        // Debounce + toggle-on-press:
        // - Wait a moment for bounce to settle
        // - Toggle only when we observe a stable transition to "pressed"
        vTaskDelay(pdMS_TO_TICKS(30));
        const int lvl = gpio_get_level(BSP_BUTTON_UPPER_IO); // 0 = pressed (pull-up)
        if (lvl == 0) {
            if (!upper_down) {
                upper_down = true;
                const bool muted = media_toggle_mic_muted();
                board_set_mic_muted(muted);
                ESP_LOGI(TAG, "Mic %s", muted ? "MUTED" : "UNMUTED");
            }
        } else {
            upper_down = false;
        }
    }
}

void board_init(void)
{
    ESP_LOGI(TAG, "Initializing board");

    // Initialize I2C (needed for codec + touch, etc.)
    ESP_ERROR_CHECK(bsp_i2c_init());

    // Initialize audio (I2S + codec devices) via BSP.
    //
    // Option A: true AEC reference on Mic3 requires ES7210 TDM output (>= 3 mics enabled).
    // ES8311 playback expects standard I2S framing, so we run:
    // - TX: standard I2S (playback)
    // - RX: TDM (capture Mic1/Mic2 + Ref Mic3)
    const i2s_std_config_t tx_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = BSP_I2S_MCLK,
            .bclk = BSP_I2S_SCLK,
            .ws = BSP_I2S_LCLK,
            .dout = BSP_I2S_DOUT,
            .din = BSP_I2S_DSIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    const i2s_tdm_config_t rx_cfg = {
        .clk_cfg = I2S_TDM_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_TDM_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                        I2S_SLOT_MODE_STEREO,
                                                        (I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3)),
        .gpio_cfg = {
            .mclk = BSP_I2S_MCLK,
            .bclk = BSP_I2S_SCLK,
            .ws = BSP_I2S_LCLK,
            .dout = BSP_I2S_DOUT,
            .din = BSP_I2S_DSIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    ESP_ERROR_CHECK(bsp_audio_init_tx_std_rx_tdm(&tx_cfg, &rx_cfg));

    s_spk_handle = bsp_audio_codec_speaker_init();
    ESP_RETURN_VOID_ON_FALSE(s_spk_handle != NULL, TAG, "Failed to init speaker codec device");

    s_mic_handle = bsp_audio_codec_microphone_init();
    ESP_RETURN_VOID_ON_FALSE(s_mic_handle != NULL, TAG, "Failed to init microphone codec device");

    // Boost microphone input gain (in dB).
    // ES7210 gain is fairly conservative by default; increasing it improves published mic level
    // without needing to add excessive post-AEC digital gain.
    //
    // If you hear clipping/distortion, reduce this value (e.g., 18.0-24.0).
    // If it's still too quiet, increase gradually (e.g., up to ~30.0).
    esp_codec_dev_set_in_gain(s_mic_handle, 30.0f);

    // Initialize display + touch and show a static image on boot.
    board_display_init_and_show_image();

    // Upper button toggles mic mute.
    // Use BSP minimal button API (GPIO0 by default).
    s_button_queue = xQueueCreate(4, sizeof(board_button_evt_t));
    if (s_button_queue != NULL) {
        ESP_ERROR_CHECK(bsp_button_init());
        ESP_ERROR_CHECK(bsp_button_register_callback(BSP_BUTTON_UPPER, board_button_isr_cb, NULL));
        (void)xTaskCreate(board_button_task_fn, "lk_btn_mute", 4096, NULL, 6, &s_button_task);
    } else {
        ESP_LOGW(TAG, "Failed to create button queue; mic mute toggle disabled");
    }
}

void board_visualizer_set_level(float level)
{
    // Called from audio render context; must not touch LVGL.
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;
    const uint32_t q15 = (uint32_t)(level * 32767.0f);
    s_visualizer_level_q15 = (uint16_t)(q15 > 32767U ? 32767U : q15);
}