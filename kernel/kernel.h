#ifndef KERNEL_H
#define KERNEL_H

/**
 * =====================================================================
 * kernel 总初始化入口:启动内核层全部常驻服务
 * (当前仅 pubsub 发布-订阅总线,后续内核组件在 kernel_init 内追加)
 * =====================================================================
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 初始化内核层服务(幂等服务各自保证)。在 app_init 之前由 main 调用 */
void kernel_init(void);

#ifdef __cplusplus
}
#endif

#endif /* KERNEL_H */
