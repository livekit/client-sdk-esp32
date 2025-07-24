#include <math.h>
#include "esp_log.h"
#include "bsp/esp-bsp.h"

static const char* TAG = "ui";

#define VIS_WIDTH 260
#define VIS_HEIGHT 162
#define VIS_GAP 10
#define VIS_SEGMENTS 5

#define LK_PALETTE_FG1 0x3B3B3B
#define LK_PALETTE_BG1 0xF9F9F6
#define LK_PALETTE_FG_ACCENT 0x002CF2

LV_IMG_DECLARE(img_logo)
LV_IMG_DECLARE(img_waveform)

static lv_obj_t* boot_screen;
static lv_obj_t* main_screen;
static lv_obj_t* call_screen;

static void ui_acquire(void)
{
    bsp_display_lock(0);
}

static void ui_release(void)
{
    bsp_display_unlock();
}

static void ui_present_screen(lv_obj_t* scr)
{
    ui_acquire();
    lv_screen_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_IN, 500, 0, false);
    ui_release();
}

static void ev_network_connected_changed(bool connected)
{
    static bool got_initial_connection = false;
    if (!got_initial_connection && connected) {
        ui_present_screen(main_screen);
        got_initial_connection = true;
    }
}

static void ev_start_call_button_clicked(lv_event_t* ev)
{
    ui_present_screen(call_screen);
}

#if BSP_CAPS_BUTTONS
static void ev_hw_button_clicked(void *button_handle, void *ctx)
{
    bsp_button_t id = (bsp_button_t)ctx;

    // TODO: Once we support more boards, we need to check board specific button IDs.
    if (id != BSP_BUTTON_MAIN) return;

    // For Box-3, return to main screen when main button is pressed.
    // This is the red circle button under the LCD.
    ui_present_screen(main_screen);
}
#endif

static void init_boot_screen(lv_obj_t* scr)
{
    lv_obj_t *img = lv_img_create(scr);
    lv_img_set_src(img, &img_logo);
    lv_obj_center(img);
}

static void init_main_screen(lv_obj_t* scr)
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

static void init_visualizer(lv_obj_t* scr)
{
    LV_DRAW_BUF_DEFINE_STATIC(draw_buf, VIS_WIDTH, VIS_HEIGHT, LV_COLOR_FORMAT_I1);
    LV_DRAW_BUF_INIT_STATIC(draw_buf);

    lv_obj_t* canvas = lv_canvas_create(scr);
    lv_canvas_set_draw_buf(canvas, &draw_buf);
    lv_canvas_set_palette(canvas, 0, lv_color_to_32(lv_color_black(), LV_OPA_COVER));
    lv_canvas_set_palette(canvas, 1, lv_color_to_32(lv_color_hex(LK_PALETTE_BG1), LV_OPA_COVER));
    lv_canvas_fill_bg(canvas, lv_color_make(0, 0, 1), LV_OPA_COVER);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);

    float magnitude = 0.0f;
    int32_t line_width = (VIS_WIDTH - (VIS_SEGMENTS - 1) * VIS_GAP) / VIS_SEGMENTS;
    int32_t half_width = line_width / 2;

    dsc.color = lv_color_make(0, 0, 1);
    dsc.width = line_width;
    dsc.round_end = 1;
    dsc.round_start = 1;

    for (int i = 0; i < VIS_SEGMENTS; i++) {
        lv_value_precise_t x = half_width + i * (line_width + VIS_GAP);
        dsc.p1.x = dsc.p2.x = x;
        dsc.p1.y = fmax(0 + half_width, ((VIS_HEIGHT / 2) - 0.01f) * (1 - magnitude));
        dsc.p2.y = fmin(VIS_HEIGHT - half_width, ((VIS_HEIGHT / 2) + 0.01f) * (1 + magnitude));
        lv_draw_line(&layer, &dsc);
    }
    lv_canvas_finish_layer(canvas, &layer);
}

static void init_call_screen(lv_obj_t* scr)
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

    lv_obj_add_style(scr, &container_style, LV_PART_MAIN);

    init_visualizer(scr);

    lv_obj_t* status_label = lv_label_create(scr);
    lv_label_set_text(status_label, "Agent is listening, ask it a question");
    lv_obj_set_style_text_color(status_label, lv_color_hex(LK_PALETTE_FG1), LV_PART_MAIN);
}

void ui_init()
{
    ui_acquire();

    boot_screen = lv_disp_get_scr_act(NULL);
    init_boot_screen(boot_screen);

    main_screen = lv_obj_create(NULL);
    init_main_screen(main_screen);

    call_screen = lv_obj_create(NULL);
    init_call_screen(call_screen);

    lv_subject_init_bool(&ui_is_network_connected, false);
    lv_subject_subscribe_bool(&ui_is_network_connected, ev_network_connected_changed);

    ui_release();

#if BSP_CAPS_BUTTONS
    static button_handle_t handles[BSP_BUTTON_NUM] = {NULL};
    ESP_ERROR_CHECK(bsp_iot_button_create(handles, NULL, BSP_BUTTON_NUM));

    for (int i = 0; i < BSP_BUTTON_NUM; i++) {
        iot_button_register_cb(handles[i], BUTTON_PRESS_DOWN, NULL, ev_hw_button_clicked, (void *)i);
    }
#endif
}