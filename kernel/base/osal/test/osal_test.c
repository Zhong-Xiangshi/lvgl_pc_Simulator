/**
 * =====================================================================
 * OSAL 后端验证程序(独立运行,不依赖 LVGL/SDL)
 *
 * 用法:适配新后端(如 FreeRTOS)后,把根 CMakeLists.txt 中 osal 目标的
 * 源文件换成 osal_freertos.c,重编本 target 并直接运行 osal_test.exe。
 * 全部通过打印 PASS 且退出码为 0;任一失败打印 FAIL 且退出码为 1。
 *
 * 覆盖的路径:
 *   同步: 信号量 take/give/满 give 失败、递归互斥锁可重入/未持有 give 失败;
 *   异步: 任务创建、任务延时、信号量门控生产者在消费者就绪后才启动、
 *         空队列非阻塞/带超时接收、队列按序收发 N 条消息、无积压、
 *         递归互斥锁保护共享计数器。
 * 每个用例一行 [PASS]/[FAIL] 日志,可移植脚本核对。
 * =====================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include "osal.h"

/* 用例结果计数(仅主线程写入;任务线程写入的字段经信号量
 * give/take 建立 happens-before 后由主线程读取,无竞争) */
static int failures;

#define TEST_CHECK(cond, ...) do {                                        \
        if (cond) { printf("  [PASS] " __VA_ARGS__); printf("\n"); }      \
        else      { printf("  [FAIL] " __VA_ARGS__); printf("\n");        \
                    failures++; }                                         \
    } while (0)

/* ==================== 异步段共享对象 ==================== */
#define TEST_MSG_NUM   5u    /* 队列往返消息数 */
#define TEST_MSG_DELAY 200u  /* 生产者发送间隔 ms */
#define TEST_COUNT_MAX 3u    /* 计数器任务自增次数 */

static osal_queue_t *g_queue;
static osal_sem_t   *g_start_sem;      /* 门控:生产者等消费者就绪 */
static osal_sem_t   *g_done_sem;       /* 消费者完成 -> 主线程 */
static osal_sem_t   *g_counter_sem;    /* 计数任务完成 -> 主线程 */
static osal_mutex_t *g_mutex;          /* 保护 g_counter */
static int           g_counter;

/* 生产者:等门控放行后,按 TEST_MSG_DELAY 间隔发送递增消息,发完自删 */
static void producer_task(void *arg)
{
    (void)arg;
    int i;
    osal_sem_take(g_start_sem, OSAL_WAIT_FOREVER);   /* 等消费者就绪 */
    printf("[OSAL-TEST] 生产者任务启动\n");
    for (i = 0; i < (int)TEST_MSG_NUM; i++) {
        osal_task_delay_ms(TEST_MSG_DELAY);
        osal_queue_send(g_queue, &i, OSAL_WAIT_FOREVER);
    }
    printf("[OSAL-TEST] 生产者已发送 %u 条,任务结束\n", (unsigned)TEST_MSG_NUM);
}

/* 消费者:先验证两条超时路径(此时生产者被门控挡住,队列必空),
 * 放行生产者后按序接收 TEST_MSG_NUM 条,最后通知主线程并自删 */
static void consumer_task(void *arg)
{
    (void)arg;
    int i, msg, order_ok = 1;

    printf("[OSAL-TEST] 消费者任务启动\n");
    TEST_CHECK(osal_queue_recv(g_queue, &msg, OSAL_NO_WAIT) == OSAL_TIMEOUT,
               "空队列非阻塞接收返回 OSAL_TIMEOUT");
    TEST_CHECK(osal_queue_recv(g_queue, &msg, 100u) == OSAL_TIMEOUT,
               "空队列 100ms 阻塞接收超时");

    osal_sem_give(g_start_sem);   /* 放行生产者(若生产者提前启动,上面两条即失败) */
    for (i = 0; i < (int)TEST_MSG_NUM; i++) {
        /* 若队列收发异常会死等,测试挂起本身即失败信号;正常路径必收满 */
        if (osal_queue_recv(g_queue, &msg, OSAL_WAIT_FOREVER) != OSAL_OK || msg != i) {
            order_ok = 0;
            break;
        }
    }
    TEST_CHECK(order_ok, "队列按序收到 %u 条消息(0..%u)",
               (unsigned)TEST_MSG_NUM, (unsigned)(TEST_MSG_NUM - 1));
    TEST_CHECK(osal_queue_waiting(g_queue) == 0, "队列无积压(waiting == 0)");

    osal_sem_give(g_done_sem);
    printf("[OSAL-TEST] 消费者任务结束\n");
}

/* 计数器:递归互斥锁双重加锁保护共享计数器,累加后双重解锁,自删 */
static void counter_task(void *arg)
{
    (void)arg;
    int i;
    for (i = 0; i < (int)TEST_COUNT_MAX; i++) {
        osal_mutex_take(g_mutex, OSAL_WAIT_FOREVER);
        osal_mutex_take(g_mutex, OSAL_WAIT_FOREVER);   /* 递归加锁,验证可重入 */
        g_counter++;
        osal_mutex_give(g_mutex);
        osal_mutex_give(g_mutex);
        osal_task_delay_ms(100u);
    }
    osal_sem_give(g_counter_sem);
    printf("[OSAL-TEST] 计数任务完成(双重加锁 %u 次)\n", (unsigned)TEST_COUNT_MAX);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);   /* 日志实时可见,重定向也不丢 */
    printf("==== OSAL 后端验证(链接哪个 osal 后端即验证哪个) ====\n");
    printf("[OSAL-TEST] tick = %lu ms\n", (unsigned long)osal_tick_ms());

    /* ---- 同步路径:信号量(二值) ---- */
    printf("[OSAL-TEST] -- 同步:信号量 --\n");
    osal_sem_t *sem = osal_sem_create(1u, 1u);
    TEST_CHECK(sem != NULL, "二值信号量创建成功");
    TEST_CHECK(osal_sem_take(sem, OSAL_NO_WAIT) == OSAL_OK,
               "信号量可用时 NO_WAIT take 返回 OSAL_OK");
    TEST_CHECK(osal_sem_take(sem, OSAL_NO_WAIT) == OSAL_TIMEOUT,
               "信号量耗尽时 NO_WAIT take 返回 OSAL_TIMEOUT");
    TEST_CHECK(osal_sem_give(sem) == OSAL_OK && osal_sem_give(sem) == OSAL_ERR_FAIL,
               "give 补满后再次 give 返回 OSAL_ERR_FAIL");
    osal_sem_delete(sem);

    /* ---- 同步路径:递归互斥锁 ---- */
    printf("[OSAL-TEST] -- 同步:互斥锁 --\n");
    osal_mutex_t *m = osal_mutex_create();
    TEST_CHECK(m != NULL, "互斥锁创建成功");
    TEST_CHECK(osal_mutex_take(m, OSAL_NO_WAIT) == OSAL_OK, "take 返回 OSAL_OK");
    TEST_CHECK(osal_mutex_take(m, OSAL_NO_WAIT) == OSAL_OK,
               "同任务递归 take 返回 OSAL_OK(可重入)");
    TEST_CHECK(osal_mutex_give(m) == OSAL_OK && osal_mutex_give(m) == OSAL_OK,
               "对应 give 两次后锁释放");
    TEST_CHECK(osal_mutex_give(m) == OSAL_ERR_PARAM,
               "未持有互斥锁时 give 返回 OSAL_ERR_PARAM");
    osal_mutex_delete(m);

    /* ---- 参数校验(非法初值应返回 NULL) ---- */
    printf("[OSAL-TEST] -- 参数校验 --\n");
    TEST_CHECK(osal_sem_create(0u, 0u) == NULL, "信号量 max_count=0 返回 NULL");
    TEST_CHECK(osal_sem_create(2u, 3u) == NULL, "信号量 initial > max 返回 NULL");
    TEST_CHECK(osal_queue_create(0u, 4u) == NULL, "队列容量为 0 返回 NULL");
    TEST_CHECK(osal_queue_create(8u, 0u) == NULL, "队列消息尺寸为 0 返回 NULL");

    /* ---- 异步段:生产者->队列->消费者 + 门控 + 递归锁计数器 ---- */
    printf("[OSAL-TEST] -- 异步:任务/队列/门控/计数器 --\n");
    g_queue      = osal_queue_create(TEST_MSG_NUM * 2, sizeof(int));
    g_start_sem  = osal_sem_create(1u, 0u);
    g_done_sem   = osal_sem_create(1u, 0u);
    g_counter_sem = osal_sem_create(1u, 0u);
    g_mutex      = osal_mutex_create();
    if (!g_queue || !g_start_sem || !g_done_sem || !g_counter_sem || !g_mutex) {
        printf("[FAIL] 异步段对象创建失败\n");
        failures++;
    } else {
        osal_task_create(producer_task, NULL, OSAL_PRIO_NORMAL, 0);
        osal_task_create(consumer_task, NULL, OSAL_PRIO_NORMAL, 0);
        osal_task_create(counter_task,  NULL, OSAL_PRIO_NORMAL, 0);

        osal_sem_take(g_done_sem, OSAL_WAIT_FOREVER);      /* 等消费者收满 */
        osal_sem_take(g_counter_sem, OSAL_WAIT_FOREVER);   /* 等计数任务完成 */
        TEST_CHECK(g_counter == (int)TEST_COUNT_MAX,
                   "递归互斥锁保护计数器累加到 %u", (unsigned)TEST_COUNT_MAX);
    }

    /* ---- 汇总:全部 PASS 退出码 0,否则 1(供脚本判断) ---- */
    if (failures == 0) printf("==== 结果: 全部 PASS ====\n");
    else               printf("==== 结果: FAIL(失败 %d 项) ====\n", failures);
    return failures ? 1 : 0;
}
