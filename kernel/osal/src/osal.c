/**
 * =====================================================================
 * OSAL pthread 后端(winpthreads / MinGW)
 *
 * 移植指南:后续移植 FreeRTOS(如 ESP32)时,新建 osal_freertos.c 实现
 * 同一份 osal.h,各函数对应关系如下,并把根 CMakeLists.txt 中 osal
 * 目标的源文件换成 osal_freertos.c:
 *   osal_task_create   -> xTaskCreate(建议静态任务)
 *   osal_task_delete   -> vTaskDelete(可直接删除其他任务,传句柄即可)
 *   osal_task_delay_ms -> vTaskDelay(pdMS_TO_TICKS(ms))
 *   osal_tick_ms       -> pdTICKS_TO_MS(xTaskGetTickCount())
 *   osal_sem_*         -> xSemaphoreCreateBinary/Counting + xSemaphoreTake/Give
 *   osal_queue_*       -> xQueueCreate/xQueueSend/xQueueReceive/uxQueueMessagesWaiting
 *   osal_mutex_*       -> xSemaphoreCreateRecursiveMutex + xSemaphoreTakeRecursive/GiveRecursive
 *
 * 本后端关键实现说明:
 *   1) 超时路径统一为 pthread_mutex + pthread_cond + 谓词 while 循环,
 *      信号量/队列/互斥锁共用同一套语义(信号量刻意不用 sem_t:
 *      winpthreads 的 sem_t 是堆分配句柄,其 sem_timedwait 是另一条
 *      独立的时间转换路径;互斥锁刻意不用 pthread_mutex_timedlock:
 *      POSIX-2008 可选接口,各 pthread 移植支持不一)。
 *   2) winpthreads 的 pthread_cond_timedwait 以墙上时钟(CLOCK_REALTIME)
 *      为基准(pthread_condattr_setclock(CLOCK_MONOTONIC) 返回 EINVAL),
 *      因此绝对截止时间必须用 CLOCK_REALTIME 计算;CLOCK_MONOTONIC
 *      只可用于纯差值语义(如 osal_tick_ms)。
 *   3) 任务线程分离运行(pthread_attr_setdetachstate),线程结束后资源
 *      自动回收;"删除任务"= 从入口函数返回或 osal_task_delete(NULL)。
 *      pthread 不支持强制删除其他线程,跨任务删除被忽略。
 *   4) 线程优先级 best-effort:winpthreads 把 sched_priority 映射到
 *      Windows SetThreadPriority,实际仅约 5 个有效档位
 *      (0->NORMAL, 1->ABOVE_NORMAL, >=2->HIGHEST),失败静默忽略。
 *   5) 时间单位毫秒,1 tick = 1 ms。
 * =====================================================================
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>

#include "osal.h"

/* =====================================================================
 * 内部公共:带截止时间的条件变量等待
 * ===================================================================== */

/* 毫秒 -> CLOCK_REALTIME 绝对截止时间。
 * 注意:winpthreads 的 pthread_cond_timedwait 仅支持 CLOCK_REALTIME 基准
 * (内部用 GetSystemTimeAsFileTime 计算差值),故绝对时间必须按墙上时钟
 * 计算,不能用 CLOCK_MONOTONIC,否则超时计算完全错误。 */
static void osal_deadline_from_ms(uint32_t timeout_ms, struct timespec *dl)
{
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    dl->tv_sec  = now.tv_sec + (time_t)(timeout_ms / 1000u);
    dl->tv_nsec = now.tv_nsec + (long)(timeout_ms % 1000u) * 1000000L;
    if (dl->tv_nsec >= 1000000000L) {
        dl->tv_nsec -= 1000000000L;
        dl->tv_sec++;
    }
}

/* 有限等待:超时返回 OSAL_TIMEOUT(用 errno 宏比较,勿硬编码数值) */
static osal_err_t osal_cond_wait_deadline(pthread_cond_t *cond, pthread_mutex_t *lock,
                                          const struct timespec *dl)
{
    int rc = pthread_cond_timedwait(cond, lock, dl);
    if (rc == ETIMEDOUT) return OSAL_TIMEOUT;
    return (rc == 0) ? OSAL_OK : OSAL_ERR_FAIL;
}

/* 无限等待 */
static osal_err_t osal_cond_wait_forever(pthread_cond_t *cond, pthread_mutex_t *lock)
{
    return (pthread_cond_wait(cond, lock) == 0) ? OSAL_OK : OSAL_ERR_FAIL;
}

/* =====================================================================
 * 任务
 * ===================================================================== */

struct osal_task {
    pthread_t thread;
    int       prio;   /* 记录请求的优先级,便于调试 */
};

osal_task_t *osal_task_create(void (*entry)(void *arg), void *arg,
                              int prio, uint32_t stack_bytes)
{
    if (entry == NULL) return NULL;

    osal_task_t *t = calloc(1, sizeof(*t));
    if (t == NULL) return NULL;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED); /* 分离:线程结束自动回收 */
    if (stack_bytes != 0) pthread_attr_setstacksize(&attr, stack_bytes); /* 尽力而为,失败忽略 */

    /* 函数指针强转:任务入口返回 void,按 pthread 惯例转为 void *(*)(void *)
     * (标准 C 不允许直接转换返回类型不同的函数指针,这里显式转换) */
    if (pthread_create(&t->thread, &attr, (void *(*)(void *))entry, arg) != 0) {
        pthread_attr_destroy(&attr);
        free(t);
        return NULL;
    }
    pthread_attr_destroy(&attr);

    if (prio > 0) {
        /* 优先级 best-effort:winpthreads 仅映射到约 5 档 Windows 线程优先级 */
        struct sched_param sp;
        sp.sched_priority = (prio > OSAL_PRIO_HIGHEST) ? OSAL_PRIO_HIGHEST : prio;
        pthread_setschedparam(t->thread, SCHED_OTHER, &sp); /* 失败忽略 */
    }
    t->prio = prio;
    return t;
}

void osal_task_delete(osal_task_t *task)
{
    if (task == NULL) {
        /* 删除调用者自身,不会返回 */
        pthread_exit(NULL);
        return;
    }
    /* pthread 后端不支持强制删除其他线程:分离线程无法 join;用 pthread_cancel
     * (延迟取消)可能让目标线程带着内部锁退出导致死锁。打印警告后忽略,
     * FreeRTOS 移植可直接 vTaskDelete(task)。 */
    fprintf(stderr, "[OSAL] 警告: pthread 后端不支持删除其他任务,已忽略\n");
}

void osal_task_delay_ms(uint32_t ms)
{
    struct timespec ts;
    ts.tv_sec  = (time_t)(ms / 1000u);
    ts.tv_nsec = (long)(ms % 1000u) * 1000000L;
    nanosleep(&ts, NULL); /* winpthreads 提供 POSIX nanosleep */
}

uint32_t osal_tick_ms(void)
{
    struct timespec ts;
    /* 此处仅做差值/回绕语义,可用单调钟(与 cond timedwait 的
     * 绝对时间基准无关,勿混淆) */
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u);
}

/* =====================================================================
 * 信号量(mutex + cond + count;不用 sem_t,见文件顶部说明)
 * ===================================================================== */

struct osal_sem {
    pthread_mutex_t lock;
    pthread_cond_t  cond;
    uint32_t        count;
    uint32_t        max_count;
};

osal_sem_t *osal_sem_create(uint32_t max_count, uint32_t initial_count)
{
    if (max_count == 0 || initial_count > max_count) return NULL;

    osal_sem_t *s = calloc(1, sizeof(*s));
    if (s == NULL) return NULL;

    pthread_mutex_init(&s->lock, NULL);
    pthread_cond_init(&s->cond, NULL);
    s->count     = initial_count;
    s->max_count = max_count;
    return s;
}

osal_err_t osal_sem_take(osal_sem_t *sem, uint32_t timeout_ms)
{
    if (sem == NULL) return OSAL_ERR_PARAM;

    pthread_mutex_lock(&sem->lock);
    if (sem->count > 0) {          /* 快速路径 */
        sem->count--;
        pthread_mutex_unlock(&sem->lock);
        return OSAL_OK;
    }
    if (timeout_ms == OSAL_NO_WAIT) {
        pthread_mutex_unlock(&sem->lock);
        return OSAL_TIMEOUT;
    }

    struct timespec dl;
    osal_deadline_from_ms(timeout_ms, &dl);  /* 只算一次截止时间,循环中复用 */
    osal_err_t rc = OSAL_OK;
    while (sem->count == 0) {                /* 谓词循环,吸收假唤醒 */
        rc = (timeout_ms == OSAL_WAIT_FOREVER)
                 ? osal_cond_wait_forever(&sem->cond, &sem->lock)
                 : osal_cond_wait_deadline(&sem->cond, &sem->lock, &dl);
        if (rc != OSAL_OK) break;
    }
    if (rc == OSAL_OK) sem->count--;
    pthread_mutex_unlock(&sem->lock);
    return rc;
}

osal_err_t osal_sem_give(osal_sem_t *sem)
{
    if (sem == NULL) return OSAL_ERR_PARAM;

    pthread_mutex_lock(&sem->lock);
    if (sem->count >= sem->max_count) {      /* 已满:同 FreeRTOS give 返回 pdFALSE */
        pthread_mutex_unlock(&sem->lock);
        return OSAL_ERR_FAIL;
    }
    sem->count++;
    pthread_cond_signal(&sem->cond);         /* 最多放行一个等待者,signal 足够 */
    pthread_mutex_unlock(&sem->lock);
    return OSAL_OK;
}

void osal_sem_delete(osal_sem_t *sem)
{
    if (sem == NULL) return;
    pthread_cond_destroy(&sem->cond);
    pthread_mutex_destroy(&sem->lock);
    free(sem);
}

/* =====================================================================
 * 消息队列(环形缓冲 + mutex + 2 cond;按值拷贝,FIFO)
 * ===================================================================== */

struct osal_queue {
    pthread_mutex_t lock;
    pthread_cond_t  not_empty;  /* 有数据可读 */
    pthread_cond_t  not_full;   /* 有空位可写 */
    uint8_t        *buf;        /* 环形缓冲:item_count * item_size 字节 */
    uint32_t        item_count;
    uint32_t        item_size;
    uint32_t        head;       /* 队首(下一次 recv 位置) */
    uint32_t        tail;       /* 队尾(下一次 send 位置) */
    uint32_t        used;       /* 当前消息条数 */
};

osal_queue_t *osal_queue_create(uint32_t item_count, uint32_t item_size)
{
    if (item_count == 0 || item_size == 0) return NULL;

    osal_queue_t *q = calloc(1, sizeof(*q));
    if (q == NULL) return NULL;

    q->buf = malloc((size_t)item_count * item_size);
    if (q->buf == NULL) {
        free(q);
        return NULL;
    }

    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
    q->item_count = item_count;
    q->item_size  = item_size;
    return q;
}

osal_err_t osal_queue_send(osal_queue_t *q, const void *item, uint32_t timeout_ms)
{
    if (q == NULL || item == NULL) return OSAL_ERR_PARAM;

    pthread_mutex_lock(&q->lock);
    if (q->used < q->item_count) goto write;   /* 快速路径 */
    if (timeout_ms == OSAL_NO_WAIT) {
        pthread_mutex_unlock(&q->lock);
        return OSAL_TIMEOUT;
    }

    {
        struct timespec dl;
        osal_deadline_from_ms(timeout_ms, &dl);
        osal_err_t rc = OSAL_OK;
        while (q->used >= q->item_count) {     /* 等空位 */
            rc = (timeout_ms == OSAL_WAIT_FOREVER)
                     ? osal_cond_wait_forever(&q->not_full, &q->lock)
                     : osal_cond_wait_deadline(&q->not_full, &q->lock, &dl);
            if (rc != OSAL_OK) break;
        }
        if (rc != OSAL_OK) {
            pthread_mutex_unlock(&q->lock);
            return rc;
        }
    }

write:
    memcpy(q->buf + (size_t)q->tail * q->item_size, item, q->item_size);
    q->tail = (q->tail + 1) % q->item_count;
    q->used++;
    pthread_cond_signal(&q->not_empty);        /* 放行一个等待接收者 */
    pthread_mutex_unlock(&q->lock);
    return OSAL_OK;
}

osal_err_t osal_queue_recv(osal_queue_t *q, void *item, uint32_t timeout_ms)
{
    if (q == NULL || item == NULL) return OSAL_ERR_PARAM;

    pthread_mutex_lock(&q->lock);
    if (q->used > 0) goto read;                /* 快速路径 */
    if (timeout_ms == OSAL_NO_WAIT) {
        pthread_mutex_unlock(&q->lock);
        return OSAL_TIMEOUT;
    }

    {
        struct timespec dl;
        osal_deadline_from_ms(timeout_ms, &dl);
        osal_err_t rc = OSAL_OK;
        while (q->used == 0) {                 /* 等数据 */
            rc = (timeout_ms == OSAL_WAIT_FOREVER)
                     ? osal_cond_wait_forever(&q->not_empty, &q->lock)
                     : osal_cond_wait_deadline(&q->not_empty, &q->lock, &dl);
            if (rc != OSAL_OK) break;
        }
        if (rc != OSAL_OK) {
            pthread_mutex_unlock(&q->lock);
            return rc;
        }
    }

read:
    memcpy(item, q->buf + (size_t)q->head * q->item_size, q->item_size);
    q->head = (q->head + 1) % q->item_count;
    q->used--;
    pthread_cond_signal(&q->not_full);         /* 放行一个等待发送者 */
    pthread_mutex_unlock(&q->lock);
    return OSAL_OK;
}

uint32_t osal_queue_waiting(osal_queue_t *q)
{
    if (q == NULL) return 0;
    pthread_mutex_lock(&q->lock);
    uint32_t used = q->used;
    pthread_mutex_unlock(&q->lock);
    return used;
}

void osal_queue_delete(osal_queue_t *q)
{
    if (q == NULL) return;
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
    pthread_mutex_destroy(&q->lock);
    free(q->buf);
    free(q);
}

/* =====================================================================
 * 互斥锁(递归;cond 实现,刻意不用 pthread_mutex_timedlock:
 * 该接口为 POSIX-2008 可选,各 pthread 移植支持不一,且本方案与
 * 信号量/队列共用同一条已验证的超时路径)
 * ===================================================================== */

struct osal_mutex {
    pthread_mutex_t lock;   /* 保护内部字段 */
    pthread_cond_t  cond;   /* 等待锁被释放 */
    pthread_t       owner;  /* 当前持有者(pthread_equal 比较) */
    uint8_t         owned;  /* 是否被持有 */
    uint32_t        depth;  /* 递归深度 */
};

osal_mutex_t *osal_mutex_create(void)
{
    osal_mutex_t *m = calloc(1, sizeof(*m));
    if (m == NULL) return NULL;

    pthread_mutex_init(&m->lock, NULL);
    pthread_cond_init(&m->cond, NULL);
    return m;
}

osal_err_t osal_mutex_take(osal_mutex_t *m, uint32_t timeout_ms)
{
    if (m == NULL) return OSAL_ERR_PARAM;

    pthread_mutex_lock(&m->lock);
    /* 可获取:未被持有,或调用者就是持有者(递归加锁) */
    if (!m->owned || pthread_equal(m->owner, pthread_self())) goto acquire;
    if (timeout_ms == OSAL_NO_WAIT) {
        pthread_mutex_unlock(&m->lock);
        return OSAL_TIMEOUT;
    }

    {
        struct timespec dl;
        osal_deadline_from_ms(timeout_ms, &dl);
        osal_err_t rc = OSAL_OK;
        while (m->owned && !pthread_equal(m->owner, pthread_self())) {
            rc = (timeout_ms == OSAL_WAIT_FOREVER)
                     ? osal_cond_wait_forever(&m->cond, &m->lock)
                     : osal_cond_wait_deadline(&m->cond, &m->lock, &dl);
            if (rc != OSAL_OK) break;
        }
        if (rc != OSAL_OK) {
            pthread_mutex_unlock(&m->lock);
            return rc;
        }
    }

acquire:
    m->owner = pthread_self();
    m->owned = 1;
    m->depth++;
    pthread_mutex_unlock(&m->lock);
    return OSAL_OK;
}

osal_err_t osal_mutex_give(osal_mutex_t *m)
{
    if (m == NULL) return OSAL_ERR_PARAM;

    pthread_mutex_lock(&m->lock);
    /* 仅持有者可释放;未持有或非持有者 give 属调用错误 */
    if (!m->owned || !pthread_equal(m->owner, pthread_self())) {
        pthread_mutex_unlock(&m->lock);
        return OSAL_ERR_PARAM;
    }
    m->depth--;
    if (m->depth == 0) {
        m->owned = 0;
        pthread_cond_signal(&m->cond);   /* 唤醒一个等待者 */
    }
    pthread_mutex_unlock(&m->lock);
    return OSAL_OK;
}

void osal_mutex_delete(osal_mutex_t *m)
{
    if (m == NULL) return;
    pthread_cond_destroy(&m->cond);
    pthread_mutex_destroy(&m->lock);
    free(m);
}
