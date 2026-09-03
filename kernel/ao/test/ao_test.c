/**
 * =====================================================================
 * AO 主动对象库验证程序(独立运行,不依赖 LVGL/SDL)
 *
 * 用法:cmake --build . --target ao_test && ./ao_test.exe
 * 全部通过打印 PASS 且退出码为 0;任一失败打印 FAIL 且退出码为 1。
 *
 * 覆盖的路径:
 *   异步委托: 堆内存 data + free_fn 在 AO 线程执行/释放;
 *   同步委托: 栈上 data 在 ao_call_sync 返回前被 AO 线程修改;
 *   队列满:   投递失败立即执行 free_fn 回收 data(调用线程);
 *   事件循环: ao_process 毫秒超时(处理返回 1 / 超时返回 0),
 *             停止消息后任务退出、ao_deinit。
 * 每个用例一行 [PASS]/[FAIL] 日志,可移植脚本核对。
 * =====================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include "ao.h"

/* 用例结果计数(仅主线程写入;AO 线程写入的字段经信号量
 * give/take 建立 happens-before 后由主线程读取,无竞争) */
static int failures;

#define TEST_CHECK(cond, ...) do {                                        \
        if (cond) { printf("  [PASS] " __VA_ARGS__); printf("\n"); }      \
        else      { printf("  [FAIL] " __VA_ARGS__); printf("\n");        \
                    failures++; }                                         \
    } while (0)

/* ==================== AO 实例与任务 ==================== */
#define AO_QUEUE_LEN 10u
#define AO_IDLE_MS   10u    /* 事件循环空闲轮询间隔 */
#define AO_WAIT_MS   2000u  /* 主线程等 AO 线程回应的超时 */

static ao_t         g_ao;
static int          g_running;        /* 仅 AO 线程读写:停止消息置 0,循环读 */
static osal_sem_t  *g_async_done;     /* 异步用例:fn+free_fn 执行完 -> 主线程 */
static osal_sem_t  *g_task_done;      /* AO 任务退出 -> 主线程 */

typedef struct {
    int magic;
} async_data_t;

static int g_async_ran;    /* fn 已在 AO 线程执行 */
static int g_async_freed;  /* free_fn 已释放 data */

static void async_fn(void *data)
{
    async_data_t *d = (async_data_t *)data;
    if (d->magic == 0x5A5A) g_async_ran = 1;
}

/* free_fn 在 fn 之后由同一 AO 线程执行,完成信号在此发出,
 * 主线程收到后 fn/free_fn 的写均可见 */
static void async_free(void *data)
{
    g_async_freed = 1;
    osal_sem_give(g_async_done);
    free(data);
}

/* 队列满回收用例:free_fn 在调用线程(主线程)立即执行 */
static int g_reclaim_freed;

static void reclaim_free(void *data)
{
    g_reclaim_freed = 1;
    free(data);
}

/* 同步用例:直接读写调用者栈上变量 */
static void sync_fn(void *data)
{
    int *v = (int *)data;
    *v = *v * 2;
}

/* 停止消息:结束事件循环 */
static void stop_fn(void *data)
{
    (void)data;
    g_running = 0;
}

/* AO 线程:事件循环,退出后通知主线程并自删(entry 返回即自删) */
static void ao_task_entry(void *arg)
{
    (void)arg;
    g_running = 1;
    printf("[AO-TEST] AO 任务启动\n");
    while (g_running) {
        ao_process(&g_ao, AO_IDLE_MS);
    }
    osal_sem_give(g_task_done);
    printf("[AO-TEST] AO 任务结束\n");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);   /* 日志实时可见,重定向也不丢 */
    printf("==== AO 主动对象库验证 ====\n");

    /* ---- 参数防御 ---- */
    printf("[AO-TEST] -- 防御检查 --\n");
    TEST_CHECK(ao_post_async(NULL, NULL, NULL, NULL) == -1, "NULL 实例投递返回 -1");
    TEST_CHECK(ao_call_sync(NULL, NULL, NULL) == -1, "NULL 实例同步调用返回 -1");
    TEST_CHECK(ao_process(NULL, 10u) == -1, "NULL 实例 ao_process 返回 -1");

    /* ---- 建 AO 实例 + 起任务 ---- */
    printf("[AO-TEST] -- 异步委托(堆 data + free_fn) --\n");
    g_async_done = osal_sem_create(1u, 0u);
    g_task_done  = osal_sem_create(1u, 0u);
    if (ao_init(&g_ao, AO_QUEUE_LEN) != 0 || g_async_done == NULL || g_task_done == NULL) {
        printf("[FAIL] AO 对象创建失败\n");
        failures++;
    } else {
        osal_task_create(ao_task_entry, NULL, OSAL_PRIO_NORMAL, 0);

        async_data_t *d = (async_data_t *)malloc(sizeof(async_data_t));
        d->magic = 0x5A5A;
        TEST_CHECK(ao_post_async(&g_ao, async_fn, d, async_free) == 0, "异步投递成功");
        TEST_CHECK(osal_sem_take(g_async_done, AO_WAIT_MS) == OSAL_OK,
                   "AO 线程执行 fn 并释放 data");
        TEST_CHECK(g_async_ran == 1 && g_async_freed == 1,
                   "fn 执行且 free_fn 释放(1/1)");

        /* ---- 同步委托(栈上 data) ---- */
        printf("[AO-TEST] -- 同步委托(栈 data) --\n");
        int v = 21;
        TEST_CHECK(ao_call_sync(&g_ao, sync_fn, &v) == 0, "同步调用返回 0");
        TEST_CHECK(v == 42, "AO 线程修改调用者栈数据(v == 42)");

        /* ---- 队列满:投递失败立即回收 ---- */
        printf("[AO-TEST] -- 队列满回收与超时 --\n");
        ao_t ao_full;
        ao_init(&ao_full, 1u);
        TEST_CHECK(ao_post_async(&ao_full, NULL, NULL, NULL) == 0,
                   "空队列(容量1)首条投递成功");
        async_data_t *d2 = (async_data_t *)malloc(sizeof(async_data_t));
        d2->magic = 0x5A5A;
        TEST_CHECK(ao_post_async(&ao_full, async_fn, d2, reclaim_free) == -1,
                   "队列满投递返回 -1");
        TEST_CHECK(g_reclaim_freed == 1, "投递失败立即执行 free_fn 回收 data");
        TEST_CHECK(ao_process(&ao_full, 50u) == 1, "ao_process 消费一条消息返回 1");
        TEST_CHECK(ao_process(&ao_full, 50u) == 0, "空队列 ao_process 50ms 超时返回 0");
        ao_deinit(&ao_full);

        /* ---- 停止消息:任务退出、销毁实例 ---- */
        printf("[AO-TEST] -- 停止与销毁 --\n");
        TEST_CHECK(ao_post_async(&g_ao, stop_fn, NULL, NULL) == 0, "投递停止消息成功");
        TEST_CHECK(osal_sem_take(g_task_done, AO_WAIT_MS) == OSAL_OK, "AO 任务退出");
        ao_deinit(&g_ao);
    }

    /* ---- 汇总:全部 PASS 退出码 0,否则 1(供脚本判断) ---- */
    if (failures == 0) printf("==== 结果: 全部 PASS ====\n");
    else               printf("==== 结果: FAIL(失败 %d 项) ====\n", failures);
    return failures ? 1 : 0;
}
