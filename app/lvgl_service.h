#ifndef LVGL_SERVICE_H
#define LVGL_SERVICE_H

/**
 * LVGL 服务:LVGL 全部运行在独立的 AO 线程中。
 * 其他线程通过委托函数与 LVGL 交互,禁止直接调用 LVGL API。
 */

#include "ao.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 初始化 LVGL 服务:创建 AO 线程,在其内部完成 LVGL/驱动/UI
 *         初始化并进入事件循环(该函数不阻塞) */
void lvgl_service_init(void);

/** @brief 同步委托 LVGL 线程执行 fn(data),阻塞直到执行完毕(data 可用栈变量) */
int lvgl_service_call_sync(ao_fn_t fn, void *data);

/** @brief 异步委托 LVGL 线程执行 fn(data),执行完后用 free_fn 释放 data
 *         (data 由调用方动态分配,free_fn 可为 NULL) */
int lvgl_service_post_async(ao_fn_t fn, void *data, ao_free_fn_t free_fn);

#ifdef __cplusplus
}
#endif

#endif /* LVGL_SERVICE_H */
