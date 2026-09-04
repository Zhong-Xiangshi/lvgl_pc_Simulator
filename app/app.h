#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 应用入口:初始化各服务模块(LVGL 服务等在独立 AO 线程中运行) */
void app_init(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_H */
