#include <math.h>
#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "livekit.h"
#include "ui.h"

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

LV_FONT_DECLARE(public_sans_medium_16);
LV_FONT_DECLARE(commit_mono_700_14);

typedef enum {
    SCREEN_BOOT,
    SCREEN_MAIN,
    SCREEN_CALL
} screen_t;

#define SCREEN_NUM 3
static lv_obj_t* screens[SCREEN_NUM];

static lv_style_t style_btn_base;
static lv_style_t style_btn_pressed;

lv_subject_t ui_is_network_connected;
lv_subject_t ui_room_state;
lv_subject_t ui_is_call_active;

void ui_acquire(void)
{
    bsp_display_lock(0);
}

void ui_release(void)
{
    bsp_display_unlock();
}

static void ui_present_screen(screen_t target)
{
    bool is_call_active = target == SCREEN_CALL;
    lv_subject_set_int(&ui_is_call_active, is_call_active);
    lv_screen_load_anim(screens[target], LV_SCR_LOAD_ANIM_FADE_IN, 500, 0, false);
}

static void ev_network_connected_changed(lv_observer_t* observer, lv_subject_t* subject)
{
    static bool got_initial_connection = false;
    int32_t is_connected = lv_subject_get_int(subject);

    if (!got_initial_connection && is_connected) {
        ui_acquire();
        ui_present_screen(SCREEN_MAIN);
        ui_release();
        got_initial_connection = true;
    }
}

static void ev_room_state_changed(lv_observer_t* observer, lv_subject_t* subject)
{
    livekit_connection_state_t room_state = (livekit_connection_state_t)lv_subject_get_int(subject);
    ESP_LOGI(TAG, "Room state: %s", livekit_connection_state_str(room_state));
}

static void ev_start_call_button_clicked(lv_event_t* ev)
{
    ui_present_screen(SCREEN_CALL);
}

#if BSP_CAPS_BUTTONS
static void ev_hw_button_clicked(void *button_handle, void *ctx)
{
    bsp_button_t id = (bsp_button_t)ctx;

    // TODO: Once we support more boards, we need to check board specific button IDs.
    if (id != BSP_BUTTON_MAIN) return;

    // For Box-3, return to main screen when main button is pressed.
    // This is the red circle button under the LCD.
    ui_present_screen(SCREEN_MAIN);
}
#endif

static void init_global_styles(void)
{
    static lv_style_transition_dsc_t btn_transition;
    static const lv_style_prop_t btn_transition_props[] = {LV_STYLE_BG_OPA, 0};
    lv_style_transition_dsc_init(&btn_transition, btn_transition_props, lv_anim_path_linear, 100, 0, NULL);

    lv_style_init(&style_btn_base);
    lv_style_set_radius(&style_btn_base, 24);
    lv_style_set_bg_opa(&style_btn_base, LV_OPA_COVER);
    lv_style_set_bg_color(&style_btn_base, lv_color_hex(LK_PALETTE_FG_ACCENT));
    lv_style_set_text_color(&style_btn_base, lv_color_white());
    lv_style_set_text_font(&style_btn_base, &commit_mono_700_14);
    lv_style_set_text_letter_space(&style_btn_base, 1);

    lv_style_set_transition(&style_btn_base, &btn_transition);

    lv_style_init(&style_btn_pressed);
    lv_style_set_bg_opa(&style_btn_pressed, LV_OPA_70);
}

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

    lv_obj_add_style(scr, &container_style, LV_PART_MAIN);

    lv_obj_t *img = lv_img_create(scr);
    lv_img_set_src(img, &img_waveform);

    lv_obj_t* label = lv_label_create(scr);
    lv_label_set_text_static(label, "Chat live with your voice AI agent");
    lv_obj_set_style_text_font(label, &public_sans_medium_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(scr, lv_color_hex(LK_PALETTE_FG1), LV_PART_MAIN);

    lv_obj_t* btn = lv_button_create(scr);
    lv_obj_remove_style_all(btn);
    lv_obj_add_style(btn, &style_btn_base, LV_PART_MAIN);
    lv_obj_add_style(btn, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_set_size(btn, 232, 44);
    lv_obj_add_event_cb(btn, ev_start_call_button_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t* btn_label = lv_label_create(btn);
    lv_label_set_text_static(btn_label, "START CALL");
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
    lv_label_set_text_static(status_label, "Agent is listening, ask it a question");
    lv_obj_set_style_text_font(status_label, &public_sans_medium_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(status_label, lv_color_hex(LK_PALETTE_FG1), LV_PART_MAIN);
}

void ui_init()
{
    ui_acquire();

    init_global_styles();

    screens[SCREEN_BOOT] = lv_disp_get_scr_act(NULL);
    init_boot_screen(screens[SCREEN_BOOT]);

    screens[SCREEN_MAIN] = lv_obj_create(NULL);
    init_main_screen(screens[SCREEN_MAIN]);

    screens[SCREEN_CALL] = lv_obj_create(NULL);
    init_call_screen(screens[SCREEN_CALL]);

    lv_subject_init_int(&ui_is_network_connected, false);
    lv_subject_init_int(&ui_room_state, LIVEKIT_CONNECTION_STATE_DISCONNECTED);
    lv_subject_init_int(&ui_is_call_active, false);

    lv_subject_add_observer(&ui_is_network_connected, ev_network_connected_changed, NULL);
    lv_subject_add_observer(&ui_room_state, ev_room_state_changed, NULL);

    ui_release();

#if BSP_CAPS_BUTTONS
    static button_handle_t handles[BSP_BUTTON_NUM] = {NULL};
    ESP_ERROR_CHECK(bsp_iot_button_create(handles, NULL, BSP_BUTTON_NUM));

    for (int i = 0; i < BSP_BUTTON_NUM; i++) {
        iot_button_register_cb(handles[i], BUTTON_PRESS_DOWN, NULL, ev_hw_button_clicked, (void *)i);
    }
#endif
}