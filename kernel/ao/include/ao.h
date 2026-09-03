#ifndef AO_H
#define AO_H

/**
 * =====================================================================
 * AO 主动对象库(单头文件,static inline,仅依赖 osal)
 *
 * 把"队列 + 委托函数"的线程通信模式封装成可复用接口:
 *   - 每个 AO 线程持有自己的 ao_t 实例(可作模块内 static 变量);
 *   - 其他线程用 ao_post_async(异步) / ao_call_sync(同步)投递委托;
 *   - AO 线程在 while(1) 中调用 ao_process(ao, 毫秒) 消费并执行;
 *   - 消息按值拷贝入队(data 只拷贝指针,指向的内存由消息自带约定负责);
 * =====================================================================
 */

#include <stdio.h>
#include "osal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ao_fn_t)(void *data);
typedef void (*ao_free_fn_t)(void *data);

/* 内部消息:ao_post_async / ao_call_sync 构造,ao_process 消费 */
typedef struct {
    ao_fn_t      fn;
    void        *data;
    ao_free_fn_t free_fn;    /* 异步:释放动态分配的 data,可为 NULL */
    osal_sem_t  *sync_sem;   /* 同步:通知调用者执行完毕,异步为 NULL */
} ao_msg_t;

typedef struct {
    osal_queue_t *queue;
} ao_t;

/* 可覆盖配置:包含本头文件前 #define 同名宏可覆盖默认值 */
#ifndef AO_SYNC_SEND_TIMEOUT_MS
#define AO_SYNC_SEND_TIMEOUT_MS 100u  /* 同步投递等队列空位的超时(毫秒) */
#endif
#ifndef AO_LOG
#define AO_LOG(...) printf(__VA_ARGS__)  /* 诊断打印,可覆盖为空 */
#endif

/* ==================== 初始化 / 销毁 ==================== */

/** @brief 初始化 AO 实例,创建容量为 queue_len 的消息队列。
 *  @return 0 成功;-1 队列创建失败 */
static inline int ao_init(ao_t *ao, uint32_t queue_len)
{
    if (ao == NULL) return -1;
    ao->queue = osal_queue_create(queue_len, sizeof(ao_msg_t));
    if (ao->queue == NULL) {
        AO_LOG("AO queue create failed\n");
        return -1;
    }
    return 0;
}

/** @brief 销毁 AO 实例(须确认无调用者正在阻塞等待) */
static inline void ao_deinit(ao_t *ao)
{
    if (ao == NULL) return;
    osal_queue_delete(ao->queue);  /* NULL 安全 */
    ao->queue = NULL;
}

/* ==================== 异步委托 ==================== */

/** @brief 委托异步任务。data 由调用方动态分配,执行完用 free_fn 释放,free_fn 可为 NULL */
static inline int ao_post_async(ao_t *ao, ao_fn_t fn, void *data, ao_free_fn_t free_fn)
{
    if (ao == NULL || ao->queue == NULL) {
        AO_LOG("AO queue is NULL, cannot post\n");
        return -1;
    }
    ao_msg_t msg = { .fn = fn, .data = data, .free_fn = free_fn, .sync_sem = NULL };
    if (osal_queue_send(ao->queue, &msg, OSAL_NO_WAIT) != OSAL_OK) {
        AO_LOG("AO queue send failed\n");
        if (free_fn != NULL) free_fn(data);  /* 投递失败立即回收,防泄漏 */
        return -1;
    }
    return 0;
}

/* ==================== 同步委托 ==================== */

/** @brief 委托同步任务并等待执行完毕。
 *         data 可用调用者栈上变量(调用者阻塞期间由 AO 线程读写) */
static inline int ao_call_sync(ao_t *ao, ao_fn_t fn, void *data)
{
    if (ao == NULL || ao->queue == NULL) {
        AO_LOG("AO queue is NULL, cannot call\n");
        return -1;
    }
    osal_sem_t *sem = osal_sem_create(1, 0);
    if (sem == NULL) {
        AO_LOG("AO sync semaphore create failed\n");
        return -1;
    }
    ao_msg_t msg = { .fn = fn, .data = data, .free_fn = NULL, .sync_sem = sem };
    if (osal_queue_send(ao->queue, &msg, AO_SYNC_SEND_TIMEOUT_MS) != OSAL_OK) {
        osal_sem_delete(sem);
        return -1;
    }
    osal_sem_take(sem, OSAL_WAIT_FOREVER);
    osal_sem_delete(sem);
    return 0;
}

/* ==================== 事件循环 ==================== */

/** @brief 消费并执行一条委托(应在 AO 线程的循环中调用)。
 *         timeout_ms 内无消息则返回,线程可趁机做别的活(喂狗、刷新界面等)。
 *  @return 1=处理了一条消息;0=等待超时无消息;-1=队列无效 */
static inline int ao_process(ao_t *ao, uint32_t timeout_ms)
{
    if (ao == NULL || ao->queue == NULL) return -1;
    ao_msg_t msg;
    if (osal_queue_recv(ao->queue, &msg, timeout_ms) != OSAL_OK) return 0;
    if (msg.fn != NULL) msg.fn(msg.data);
    if (msg.sync_sem != NULL) {
        osal_sem_give(msg.sync_sem);        /* 同步:唤醒等待的调用者 */
    } else if (msg.free_fn != NULL) {
        msg.free_fn(msg.data);              /* 异步:清理动态分配的内存 */
    }
    return 1;
}

#ifdef __cplusplus
}
#endif

#endif /* AO_H */
