#include "app.h"
#include "kernel.h"
#include "osal.h"

int main(int argc, char**argv){
    (void)argc;
    (void)argv;
    kernel_init(); /* 内核层服务:pubsub broker 线程等 */
    app_init();    /* 应用层服务线程,LVGL 在独立 AO 线程中运行 */

    /* 主线程无业务,挂起自身;进程生命周期由各服务线程维持 */
    osal_sem_t *hold = osal_sem_create(1u, 0u);
    osal_sem_take(hold, OSAL_WAIT_FOREVER);
    return 0;
}
