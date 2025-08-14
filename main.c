#include <stdio.h>
#include <lvgl.h>
#include <SDL.h>
#include <ui.h>

void ui_driver_init(void) {
    /* 创建一个 SDL 窗口作为 LVGL 显示 */
    int width = 480, height = 480;
    lv_display_t *disp = lv_sdl_window_create(width, height);

    /* 创建输入设备（鼠标/键盘/滚轮） */
    lv_indev_t *mouse = lv_sdl_mouse_create(); 
    lv_indev_t *kb    = lv_sdl_keyboard_create(); 
    lv_indev_t *wheel = lv_sdl_mousewheel_create(); 
}


int main(int argc, char**argv){
    lv_init();
    ui_driver_init(); // 初始化驱动
    ui_init(); // 初始化 UI
    lv_tick_set_cb(SDL_GetTicks);
    while (1) {
        lv_timer_handler();   // 处理 LVGL 任务
        SDL_Delay(2);
    }
    return 0;
}
