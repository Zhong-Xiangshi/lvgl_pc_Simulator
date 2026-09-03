#ifndef OSAL_H
#define OSAL_H

/**
 * =====================================================================
 * OSAL 操作系统抽象层(FreeRTOS 风格)
 *
 * 提供任务 / 信号量 / 消息队列 / 互斥锁四类抽象,语义对齐 FreeRTOS:
 *   - 句柄式 API,对象动态创建;
 *   - 阻塞操作带毫秒超时(OSAL_WAIT_FOREVER / OSAL_NO_WAIT);
 *   - 队列按值拷贝(FIFO);
 *   - 互斥锁可递归(同 FreeRTOS mutex);
 *   - 时间单位统一为毫秒,1 tick = 1 ms。
 *
 * 本头文件即移植接缝:PC 端由 kernel/osal/src/osal.c 以 pthread 实现;
 * 移植 FreeRTOS(如 ESP32)时新建 osal_freertos.c 实现同一份 API,
 * 应用层代码无需改动。
 *
 * 注意:若宿主工程(LVGL)为单线程配置,OSAL 任务内禁止调用其 API。
 * =====================================================================
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 返回码 ==================== */
typedef enum {
    OSAL_OK        = 0,  /* 成功 */
    OSAL_TIMEOUT   = 1,  /* 等待超时;非阻塞操作因条件不满足未执行时也返回此码 */
    OSAL_ERR_PARAM = 2,  /* 参数错误(NULL 句柄、非法初值等) */
    OSAL_ERR_FAIL  = 3,  /* 其他失败(资源不足、状态不允许,如对已满信号量 give) */
} osal_err_t;

/* ==================== 时间与优先级 ==================== */
/* 阻塞操作的超时参数,单位毫秒 */
#define OSAL_WAIT_FOREVER  0xFFFFFFFFu  /* 无限等待(等价 FreeRTOS portMAX_DELAY) */
#define OSAL_NO_WAIT       0u           /* 立即返回,不阻塞 */

/* 任务优先级:0 最低,数值越大越高。pthread 后端按"尽力而为"映射到
 * Windows 线程优先级(实际仅约 5 个有效档位,详见 osal.c 顶部说明)。 */
#define OSAL_PRIO_LOWEST   0
#define OSAL_PRIO_NORMAL   1
#define OSAL_PRIO_HIGH     2
#define OSAL_PRIO_HIGHEST  15
#define OSAL_TASK_STACK_DEFAULT 0u  /* 任务栈:传 0 使用后端默认栈(FreeRTOS 端口可换静态栈) */

/* ==================== 句柄(不透明) ==================== */
typedef struct osal_task   osal_task_t;
typedef struct osal_sem    osal_sem_t;
typedef struct osal_queue  osal_queue_t;
typedef struct osal_mutex  osal_mutex_t;

/* ==================== 任务 ==================== */
/**
 * @brief 创建任务(分离运行)。
 * @param entry 任务入口函数。pthread 后端:从 entry 返回等价于任务自删除。
 * @param prio  优先级,见 OSAL_PRIO_*。
 * @param stack_bytes 任务栈大小,传 OSAL_TASK_STACK_DEFAULT 用后端默认值。
 * @return 任务句柄;失败(NULL 参数、资源不足)返回 NULL。
 */
osal_task_t *osal_task_create(void (*entry)(void *arg), void *arg,
                              int prio, uint32_t stack_bytes);

/**
 * @brief 删除任务。
 * @param task 要删除的任务;传 NULL 表示删除调用者自身(此调用不会返回)。
 *             注:pthread 后端不支持删除其他任务,传非 NULL 非自身句柄时
 *             打印警告并忽略(FreeRTOS 移植可 vTaskDelete 直接支持)。
 */
void osal_task_delete(osal_task_t *task);

/**
 * @brief 相对延时当前任务(等价 FreeRTOS vTaskDelay)。ms 后返回。
 */
void osal_task_delay_ms(uint32_t ms);

/**
 * @brief 获取单调递增的毫秒 tick(等价 FreeRTOS xTaskGetTickCount)。
 *        2^32 ms(约 49.7 天)后回绕,与 FreeRTOS tick 行为一致。
 */
uint32_t osal_tick_ms(void);

/* ==================== 信号量(二值/计数,同一种句柄) ==================== */
/**
 * @brief 创建信号量。
 * @param max_count    最大计数值:1 即二值信号量,>1 为计数信号量。
 * @param initial_count 初始计数值,须 <= max_count,否则返回 NULL。
 * @return 信号量句柄;失败返回 NULL。
 */
osal_sem_t *osal_sem_create(uint32_t max_count, uint32_t initial_count);

/**
 * @brief 获取信号量,计数值减 1。
 * @param timeout_ms 超时:OSAL_WAIT_FOREVER 无限等,OSAL_NO_WAIT 立即返回。
 * @return OSAL_OK 成功;OSAL_TIMEOUT 超时。
 */
osal_err_t osal_sem_take(osal_sem_t *sem, uint32_t timeout_ms);

/**
 * @brief 释放信号量,计数值加 1。
 * @return OSAL_OK 成功;OSAL_ERR_FAIL 信号量已满(同 FreeRTOS 对满信号量
 *         give 返回 pdFALSE 的语义)。
 */
osal_err_t osal_sem_give(osal_sem_t *sem);

/** @brief 删除信号量(NULL 安全)。删除仍在使用中的对象属未定义行为(同 FreeRTOS 约定)。 */
void osal_sem_delete(osal_sem_t *sem);

/* ==================== 消息队列(按值拷贝,FIFO) ==================== */
/**
 * @brief 创建消息队列。
 * @param item_count 队列容量(消息条数)。
 * @param item_size  单条消息字节数。
 * @return 队列句柄;参数非法或资源不足返回 NULL。
 */
osal_queue_t *osal_queue_create(uint32_t item_count, uint32_t item_size);

/**
 * @brief 发送消息到队尾(按值拷贝)。队列满时按 timeout_ms 阻塞等待空位。
 * @return OSAL_OK 成功;OSAL_TIMEOUT 超时。
 */
osal_err_t osal_queue_send(osal_queue_t *q, const void *item, uint32_t timeout_ms);

/**
 * @brief 从队首接收消息(按值拷贝)。队列空时按 timeout_ms 阻塞等待消息。
 * @return OSAL_OK 成功;OSAL_TIMEOUT 超时。
 */
osal_err_t osal_queue_recv(osal_queue_t *q, void *item, uint32_t timeout_ms);

/** @brief 当前队列中等待接收的消息条数(等价 FreeRTOS uxQueueMessagesWaiting)。 */
uint32_t osal_queue_waiting(osal_queue_t *q);

/** @brief 删除队列(NULL 安全)。删除仍在使用中的对象属未定义行为(同 FreeRTOS 约定)。 */
void osal_queue_delete(osal_queue_t *q);

/* ==================== 互斥锁(递归,同 FreeRTOS mutex 语义) ==================== */
/**
 * @brief 创建递归互斥锁:同一任务可重复 take,须对应 give 相同次数才真正释放。
 */
osal_mutex_t *osal_mutex_create(void);

/**
 * @brief 获取互斥锁(可重入)。被其他任务持有时按 timeout_ms 阻塞。
 * @return OSAL_OK 成功;OSAL_TIMEOUT 超时。
 */
osal_err_t osal_mutex_take(osal_mutex_t *m, uint32_t timeout_ms);

/**
 * @brief 释放互斥锁。
 * @return OSAL_OK 成功;OSAL_ERR_PARAM 调用者不是持有者或锁未被持有。
 */
osal_err_t osal_mutex_give(osal_mutex_t *m);

/** @brief 删除互斥锁(NULL 安全)。删除仍在使用中的对象属未定义行为(同 FreeRTOS 约定)。 */
void osal_mutex_delete(osal_mutex_t *m);

#ifdef __cplusplus
}
#endif

#endif /* OSAL_H */
