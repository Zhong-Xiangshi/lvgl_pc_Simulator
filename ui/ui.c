#include "ui.h"
#include <lvgl.h>
#include <lv_examples.h>
#include <lv_demos.h>

void ui_init(void) {

    // lv_example_anim_1();

    lv_demo_benchmark();

    /* 简单 UI*/
    // lv_obj_t *btn = lv_button_create(lv_screen_active());
    // lv_obj_center(btn);
    // lv_obj_t *label = lv_label_create(btn);
    // lv_label_set_text(label, "Hello LVGL (SDL)");
    // lv_obj_center(label);
}