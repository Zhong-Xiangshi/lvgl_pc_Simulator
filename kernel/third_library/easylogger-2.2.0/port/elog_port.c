/*
 * This file is part of the EasyLogger Library.
 *
 * Copyright (c) 2015, Armink, <armink.ztl@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * 'Software'), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Function: Portable interface for each platform.
 * Created on: 2015-04-28
 *
 * ============================================================================
 * LVGL_PC_Simulator 工程移植实现(PC 控制台输出):
 *   输出设备    -> osal_printf(OSAL 控制台通道,单次调用原子输出并刷新);
 *   输出互斥    -> osal_mutex(elog_output 格式化全程持锁,须真互斥,见 elog.c);
 *   当前时间    -> <time.h> 墙钟(被 elog 输出锁保护调用,见 elog.c elog_output);
 *   p/t_info    -> 空串:OSAL 任务暂无名字概念,桌面进程信息亦无意义。
 *
 *   FreeRTOS 移植指南:换 osal 后端后,本文件除 elog_port_get_time(改读 RTC)
 *   与 elog_port_get_t_info(可返回 pcTaskGetName(NULL))外无需改动。
 * ============================================================================
 */

#include <elog.h>

#include <string.h>
#include <time.h>

#include "osal.h"

/* 输出锁:elog_output() 在格式化共享 log_buf 期间持锁,多线程日志依赖其互斥 */
static osal_mutex_t *s_output_lock;

/**
 * EasyLogger port initialize
 *
 * @return result
 */
ElogErrCode elog_port_init(void) {
    ElogErrCode result = ELOG_NO_ERR;

    /* 失败(资源不足)属致命情形,置空后 output_lock/unlock 对 NULL 返回错误码,
     * 不崩溃;其余 elog 输出路径照常 */
    s_output_lock = osal_mutex_create();

    return result;
}

/**
 * output log port interface
 *
 * @param log output of log
 * @param size log size
 */
void elog_port_output(const char *log, size_t size) {
    /* 日志行非 NUL 结尾,用 %. *s 精确输出 size 字节;内容含 '%' 也被视为数据,
     * 不会进入格式化解析 */
    osal_printf("%.*s", (int)size, log);
}

/**
 * output lock
 */
void elog_port_output_lock(void) {
    osal_mutex_take(s_output_lock, OSAL_WAIT_FOREVER);
}

/**
 * output unlock
 */
void elog_port_output_unlock(void) {
    osal_mutex_give(s_output_lock);
}

/**
 * get current time interface
 *
 * @return current time
 */
const char *elog_port_get_time(void) {
    static char buf[32];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);   /* 在 elog 输出锁内调用,竞争可忽略 */

    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
    return buf;
}

/**
 * get current process name interface
 *
 * @return current process name
 */
const char *elog_port_get_p_info(void) {
    return "";   /* 桌面模拟无进程信息;需要时可返回进程名 */
}

/**
 * get current thread name interface
 *
 * @return current thread name
 */
const char *elog_port_get_t_info(void) {
    /* OSAL 任务暂无名字概念;FreeRTOS 移植可返回 pcTaskGetName(NULL) */
    return "";
}
