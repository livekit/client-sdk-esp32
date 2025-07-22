#include "esp_log.h"
#include "lvgl.h"

static const char* TAG = "ui";

#define LK_PALETTE_FG1 0x3B3B3B
#define LK_PALETTE_BG1 0xF9F9F6
#define LK_PALETTE_FG_ACCENT 0x002CF2

LV_IMG_DECLARE(img_waveform)

static void ev_start_call_button_clicked(lv_event_t* ev)
{
    ESP_LOGI(TAG, "Start Call button clicked");
}

void example_ui(lv_obj_t *scr)
{
    static lv_style_t container_style;
    lv_style_init(&container_style);
    lv_style_set_pad_row(&container_style, 20);
    lv_style_set_bg_color(&container_style, lv_color_hex(LK_PALETTE_BG1));
    lv_style_set_flex_flow(&container_style, LV_FLEX_FLOW_COLUMN);
    lv_style_set_flex_main_place(&container_style, LV_FLEX_ALIGN_CENTER);
    lv_style_set_flex_track_place(&container_style, LV_FLEX_ALIGN_CENTER);
    lv_style_set_flex_cross_place(&container_style, LV_FLEX_ALIGN_CENTER);
    lv_style_set_layout(&container_style, LV_LAYOUT_FLEX);

    static lv_style_transition_dsc_t start_btn_transition;
    static const lv_style_prop_t start_btn_transition_props[] = {LV_STYLE_BG_OPA, 0};
    lv_style_transition_dsc_init(&start_btn_transition, start_btn_transition_props, lv_anim_path_linear, 100, 0, NULL);

    static lv_style_t start_btn_style;
    lv_style_init(&start_btn_style);
    lv_style_set_radius(&start_btn_style, 24);
    lv_style_set_bg_opa(&start_btn_style, LV_OPA_COVER);
    lv_style_set_bg_color(&start_btn_style, lv_color_hex(LK_PALETTE_FG_ACCENT));
    lv_style_set_text_color(&start_btn_style, lv_color_white());
    lv_style_set_transition(&start_btn_style, &start_btn_transition);

    static lv_style_t start_btn_style_pressed;
    lv_style_init(&start_btn_style_pressed);
    lv_style_set_bg_opa(&start_btn_style_pressed, LV_OPA_70);

    lv_obj_add_style(scr, &container_style, LV_PART_MAIN);

    lv_obj_t *img = lv_img_create(scr);
    lv_img_set_src(img, &img_waveform);

    lv_obj_t* label = lv_label_create(scr);
    lv_label_set_text(label, "Chat live with your voice AI agent");
    lv_obj_set_style_text_color(scr, lv_color_hex(LK_PALETTE_FG1), LV_PART_MAIN);

    lv_obj_t* btn = lv_button_create(scr);
    lv_obj_remove_style_all(btn);
    lv_obj_add_style(btn, &start_btn_style, LV_PART_MAIN);
    lv_obj_add_style(btn, &start_btn_style_pressed, LV_STATE_PRESSED);
    lv_obj_set_size(btn, 232, 44);
    lv_obj_add_event_cb(btn, ev_start_call_button_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t* btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "START CALL");
    lv_obj_center(btn_label);
}