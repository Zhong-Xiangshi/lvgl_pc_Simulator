#include "kernel.h"

#include <elog.h>

#include "pubsub.h"



/** @brief 日志服务初始化(在一切内核组件之前调用,组件其后即可用 elog_* 输出) */
static void elog_log_init(void)
{
    elog_init();

    /* 各级别输出格式:ASSERT 带目录/函数/行号,便于定位;常规级别精简为 级别+标签+时间 */
    elog_set_fmt(ELOG_LVL_ASSERT, ELOG_FMT_ALL);
    elog_set_fmt(ELOG_LVL_ERROR,  ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME);
    elog_set_fmt(ELOG_LVL_WARN,   ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME);
    elog_set_fmt(ELOG_LVL_INFO,   ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME);
    elog_set_fmt(ELOG_LVL_DEBUG,   ELOG_FMT_ALL & ~(ELOG_FMT_FUNC | ELOG_FMT_T_INFO | ELOG_FMT_P_INFO));
    elog_set_fmt(ELOG_LVL_VERBOSE, ELOG_FMT_ALL & ~(ELOG_FMT_FUNC | ELOG_FMT_T_INFO | ELOG_FMT_P_INFO));

    /* ANSI 彩色日志:Windows Terminal / VS Code 终端 / git-bash 均正常;
     * 经典 cmd.exe 不支持 ANSI 会显示转义乱码,去掉此行退回纯文本 */
    elog_set_text_color_enabled(true);

    elog_start();   /* 使能输出并打印版本 banner(经异步输出线程写出) */
}

void kernel_init(void)
{
    /* 日志服务最先启动,后续内核组件即可打日志 */
    elog_log_init();

    /* 发布-订阅总线:创建 broker 线程(pubsub_init 幂等)。
     * 失败仅告警继续运行:其余 pubsub API 在未初始化时均返回失败,不崩溃 */
    if (pubsub_init() != 0) {
        elog_w("kernel", "pubsub_init 失败");
    }
}
