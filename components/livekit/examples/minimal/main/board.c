#include "esp_log.h"
#include "esp_check.h"

#include <string.h>
#include <stdio.h>

#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "lvgl.h"

#include "board.h"

static const char *TAG = "board";

// Some IDE/lint setups don't automatically include the generated `sdkconfig.h`.
// Keep this as a harmless fallback to avoid false-positive diagnostics.
#ifndef CONFIG_LK_EXAMPLE_CODEC_BOARD_TYPE
#define CONFIG_LK_EXAMPLE_CODEC_BOARD_TYPE "UNUSED"
#endif

// This example uses the Waveshare ESP32-S3-Touch-AMOLED-2.06 BSP to initialize audio I/O.
static esp_codec_dev_handle_t s_mic_handle = NULL;
static esp_codec_dev_handle_t s_spk_handle = NULL;

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

    lv_obj_t *img = lv_image_create(scr);
    lv_image_set_src(img, board_boot_png_dsc());
    lv_obj_center(img);

    lv_obj_t *label = lv_label_create(scr);
#if !LV_USE_LODEPNG
    lv_label_set_text(label, "PNG decoder disabled: enable CONFIG_LV_USE_LODEPNG");
    lv_obj_center(label);
    bsp_display_unlock();
    return;
#else
    if (wh_ok) {
        char msg[96];
        const uint64_t decoded_bytes = (uint64_t)png_w * (uint64_t)png_h * 4ULL;
        const unsigned decoded_kb = (unsigned)(decoded_bytes / 1024ULL);

        // Ask LVGL what size it resolved for the image source (0x0 usually means decode failed).
        const int32_t lv_w = lv_image_get_src_width(img);
        const int32_t lv_h = lv_image_get_src_height(img);

        snprintf(msg, sizeof(msg), "boot.png %ux%u (~%u KB) lv:%ldx%ld",
                 (unsigned)png_w, (unsigned)png_h, decoded_kb, (long)lv_w, (long)lv_h);
        lv_label_set_text(label, msg);
    } else {
        lv_label_set_text(label, "boot.png (embedded)");
    }
    lv_obj_align_to(label, img, LV_ALIGN_OUT_BOTTOM_MID, 0, 12);
#endif

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

void board_init(void)
{
    ESP_LOGI(TAG, "Initializing board");

    // Initialize I2C (needed for codec + touch, etc.)
    ESP_ERROR_CHECK(bsp_i2c_init());

    // Initialize audio (I2S + codec devices) via BSP
    const i2s_std_config_t i2s_cfg = {
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
    ESP_ERROR_CHECK(bsp_audio_init(&i2s_cfg));

    s_spk_handle = bsp_audio_codec_speaker_init();
    ESP_RETURN_VOID_ON_FALSE(s_spk_handle != NULL, TAG, "Failed to init speaker codec device");

    s_mic_handle = bsp_audio_codec_microphone_init();
    ESP_RETURN_VOID_ON_FALSE(s_mic_handle != NULL, TAG, "Failed to init microphone codec device");

    // Initialize display + touch and show a static image on boot.
    board_display_init_and_show_image();
}