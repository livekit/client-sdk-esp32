#include "lvgl.h"

void example_ui(lv_obj_t *scr)
{
    lv_obj_t * my_button1 = lv_button_create(scr);
    /*Set parent-sized width, and content-sized height*/
    lv_obj_set_size(my_button1, lv_pct(100), LV_SIZE_CONTENT);
    /*Align to the right center with 20px offset horizontally*/
    lv_obj_align(my_button1, LV_ALIGN_RIGHT_MID, -20, 0);

    lv_obj_t * my_label1 = lv_label_create(my_button1);
    lv_label_set_text_fmt(my_label1, "Click me!");
    lv_obj_set_style_text_color(my_label1, lv_color_hex(0xff0000), 0);
}