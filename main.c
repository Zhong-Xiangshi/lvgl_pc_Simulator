#include <app.h>
#include <kernel.h>
#include <osal.h>
#include <framework.h>

int main(int argc, char**argv){
    (void)argc;
    (void)argv;
    kernel_init(); 
    framework_init();
    app_init();

    /* 主线程无业务,挂起自身;进程生命周期由各服务线程维持 */
    osal_sem_t *hold = osal_sem_create(1u, 0u);
    osal_sem_take(hold, OSAL_WAIT_FOREVER);
    return 0;
}
