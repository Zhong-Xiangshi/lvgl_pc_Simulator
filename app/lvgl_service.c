#include <lvgl.h>
#include <SDL.h>
#include <ui/ui.h>
#include "lvgl_service.h"

static ao_t s_lvgl_ao;

/* 创建 SDL 窗口与输入设备(鼠标/键盘/滚轮),仅在 LVGL 线程内调用 */
static void ui_driver_init(void)
{
    int width = 480, height = 480;
    lv_display_t *disp = lv_sdl_window_create(width, height);
    lv_indev_t *mouse = lv_sdl_mouse_create();
    lv_indev_t *kb    = lv_sdl_keyboard_create();
    lv_indev_t *wheel = lv_sdl_mousewheel_create();
}

/* LVGL 线程:初始化 + 事件循环。ao_process 在等待下一个 LVGL 定时器
 * 期间阻塞收委托消息,其他线程投递的 fn 在本线程执行 */
static void lvgl_thread_entry(void *arg)
{
    (void)arg;
    lv_init();
    ui_driver_init();
    ui_init();
    lv_tick_set_cb(SDL_GetTicks);

    while (1) {
        uint32_t ms = lv_timer_handler();   /* 处理 LVGL 定时器,返回距下一个的毫秒数 */
        ao_process(&s_lvgl_ao, ms);         /* 等下一个定时器期间,处理委托消息 */
    }
}

void lvgl_service_init(void)
{
    ao_init(&s_lvgl_ao, 8u);
    osal_task_create(lvgl_thread_entry, NULL, OSAL_PRIO_NORMAL, 4*1024);
}

int lvgl_service_call_sync(ao_fn_t fn, void *data)
{
    return ao_call_sync(&s_lvgl_ao, fn, data);
}

int lvgl_service_post_async(ao_fn_t fn, void *data, ao_free_fn_t free_fn)
{
    return ao_post_async(&s_lvgl_ao, fn, data, free_fn);
}
