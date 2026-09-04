#include "kernel.h"

#include <stdio.h>

#include "pubsub.h"

void kernel_init(void)
{
    /* 发布-订阅总线:创建 broker 线程(pubsub_init 幂等)。
     * 失败仅告警继续运行:其余 pubsub API 在未初始化时均返回失败,不崩溃 */
    if (pubsub_init() != 0) {
        fprintf(stderr, "[KERNEL] pubsub_init 失败\n");
    }
}
