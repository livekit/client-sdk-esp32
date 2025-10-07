#include "bsp/esp-bsp.h"

#include "ui.h"

LV_IMG_DECLARE(img_logo)

static void init_main_screen(lv_obj_t* scr)
{
    lv_obj_t *img = lv_img_create(scr);
    lv_img_set_src(img, &img_logo);
    lv_obj_center(img);
}

void ui_acquire(void)
{
    bsp_display_lock(0);
}

void ui_release(void)
{
    bsp_display_unlock();
}

void ui_init()
{
    ui_acquire();
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), LV_PART_MAIN);
    init_main_screen(lv_disp_get_scr_act(NULL));
    ui_release();
}