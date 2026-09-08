/*
 * This file is part of the EasyLogger Library.
 *
 * Copyright (c) 2016-2017, Armink, <armink.ztl@gmail.com>
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
 * Function: Logs asynchronous output.
 * Created on: 2016-11-06
 *
 * ============================================================================
 * 本文件适配说明(LVGL_PC_Simulator 工程定制):
 *   官方异步输出线程用 pthread_attr/sem_t/sched 直接创建,绕过本工程 OSAL
 *   抽象层(内核代码只能依赖 osal.h 单接缝,便于换 FreeRTOS 后端)。
 *   适配内容(与官方 ELOG_ASYNC_OUTPUT_USING_PTHREAD 开关等价):
 *     sem_t output_notice   -> osal_sem_t(计数信号量,多次通知可合并唤醒);
 *     pthread_create(线程)  -> osal_task_create(分离线程,低优先级)。
 *   其余环形缓冲 / 按行取出 / 双 while 输出结构保持官方原样。
 *   FreeRTOS 移植:换 osal 后端即可,本文件无需改动。
 * ============================================================================
 */

#include <elog.h>
#include <string.h>

#include "osal.h"   /* 输出线程统一由 osal 创建(uthash src 同款做法) */

#ifdef ELOG_ASYNC_OUTPUT_ENABLE

/* 输出线程单次取走的日志上限(留余量给换行/取行处理,见 elog_async_get_line_log) */
#ifndef ELOG_ASYNC_POLL_GET_LOG_BUF_SIZE
#define ELOG_ASYNC_POLL_GET_LOG_BUF_SIZE         (ELOG_LINE_BUF_SIZE - 4)
#endif

/* 异步输出日志通知信号量(计数:满而线程忙时通知可合并为一次唤醒) */
static osal_sem_t *s_output_notice;
/* 异步输出线程句柄(分离运行,无需保存之外的操作) */
static osal_task_t *s_output_thread;

/* the highest output level for async mode, other level will sync output */
#ifdef ELOG_ASYNC_OUTPUT_LVL
#define OUTPUT_LVL                               ELOG_ASYNC_OUTPUT_LVL
#else
#define OUTPUT_LVL                               ELOG_LVL_ASSERT
#endif /* ELOG_ASYNC_OUTPUT_LVL */

/* buffer size for asynchronous output mode */
#ifdef ELOG_ASYNC_OUTPUT_BUF_SIZE
#define OUTPUT_BUF_SIZE                          ELOG_ASYNC_OUTPUT_BUF_SIZE
#else
#define OUTPUT_BUF_SIZE                          (ELOG_LINE_BUF_SIZE * 10)
#endif /* ELOG_ASYNC_OUTPUT_BUF_SIZE */

/* Initialize OK flag */
static bool init_ok = false;
/* asynchronous output mode enabled flag */
static bool is_enabled = false;
/* asynchronous output mode's ring buffer */
static char log_buf[OUTPUT_BUF_SIZE] = { 0 };
/* log ring buffer write index */
static size_t write_index = 0;
/* log ring buffer read index */
static size_t read_index = 0;
/* log ring buffer full flag */
static bool buf_is_full = false;
/* log ring buffer empty flag */
static bool buf_is_empty = true;

extern void elog_port_output(const char *log, size_t size);
extern void elog_output_lock(void);
extern void elog_output_unlock(void);

/**
 * asynchronous output ring buffer used size
 *
 * @return used size
 */
static size_t elog_async_get_buf_used(void) {
    if (write_index > read_index) {
        return write_index - read_index;
    } else {
        if (!buf_is_full && !buf_is_empty) {
            return OUTPUT_BUF_SIZE - (read_index - write_index);
        } else if (buf_is_full) {
            return OUTPUT_BUF_SIZE;
        } else {
            return 0;
        }
    }
}

/**
 * asynchronous output ring buffer remain space
 *
 * @return remain space
 */
static size_t async_get_buf_space(void) {
    return OUTPUT_BUF_SIZE - elog_async_get_buf_used();
}

/**
 * put log to asynchronous output ring buffer
 *
 * @param log put log buffer
 * @param size log size
 *
 * @return put log size, the log which beyond ring buffer space will be dropped
 */
static size_t async_put_log(const char *log, size_t size) {
    size_t space = 0;

    space = async_get_buf_space();
    /* no space */
    if (!space) {
        size = 0;
        goto __exit;
    }
    /* drop some log */
    if (space <= size) {
        size = space;
        buf_is_full = true;
    }

    if (write_index + size < OUTPUT_BUF_SIZE) {
        memcpy(log_buf + write_index, log, size);
        write_index += size;
    } else {
        memcpy(log_buf + write_index, log, OUTPUT_BUF_SIZE - write_index);
        memcpy(log_buf, log + OUTPUT_BUF_SIZE - write_index,
                size - (OUTPUT_BUF_SIZE - write_index));
        write_index += size - OUTPUT_BUF_SIZE;
    }

    buf_is_empty = false;

__exit:

    return size;
}

#ifdef ELOG_ASYNC_LINE_OUTPUT
/**
 * Get line log from asynchronous output ring buffer.
 * It will copy all log when the newline sign isn't find.
 *
 * @param log get line log buffer
 * @param size line log size
 *
 * @return get line log size, the log size is less than ring buffer used size
 */
size_t elog_async_get_line_log(char *log, size_t size) {
    size_t used = 0, cpy_log_size = 0;
    /* lock output */
    elog_output_lock();
    used = elog_async_get_buf_used();

    /* no log */
    if (!used || !size) {
        goto __exit;
    }
    /* less log */
    if (used <= size) {
        size = used;
    }

    if (read_index + size < OUTPUT_BUF_SIZE) {
        cpy_log_size = elog_cpyln(log, log_buf + read_index, size);
        read_index += cpy_log_size;
    } else {
        cpy_log_size = elog_cpyln(log, log_buf + read_index, OUTPUT_BUF_SIZE - read_index);
        if (cpy_log_size == OUTPUT_BUF_SIZE - read_index) {
            cpy_log_size += elog_cpyln(log + cpy_log_size, log_buf, size - cpy_log_size);
            read_index += cpy_log_size - OUTPUT_BUF_SIZE;
        } else {
            read_index += cpy_log_size;
        }
    }

    if (used == cpy_log_size) {
        buf_is_empty = true;
    }

    if (cpy_log_size) {
        buf_is_full = false;
    }

__exit:
    /* lock output */
    elog_output_unlock();
    return cpy_log_size;
}
#else
/**
 * get log from asynchronous output ring buffer
 *
 * @param log get log buffer
 * @param size log size
 *
 * @return get log size, the log size is less than ring buffer used size
 */
size_t elog_async_get_log(char *log, size_t size) {
    size_t used = 0;
    /* lock output */
    elog_output_lock();
    used = elog_async_get_buf_used();
    /* no log */
    if (!used || !size) {
        size = 0;
        goto __exit;
    }
    /* less log */
    if (used <= size) {
        size = used;
        buf_is_empty = true;
    }

    if (read_index + size < OUTPUT_BUF_SIZE) {
        memcpy(log, log_buf + read_index, size);
        read_index += size;
    } else {
        memcpy(log, log_buf + read_index, OUTPUT_BUF_SIZE - read_index);
        memcpy(log + OUTPUT_BUF_SIZE - read_index, log_buf,
                size - (OUTPUT_BUF_SIZE - read_index));
        read_index += size - OUTPUT_BUF_SIZE;
    }

    buf_is_full = false;

__exit:
    /* lock output */
    elog_output_unlock();
    return size;
}
#endif /* ELOG_ASYNC_LINE_OUTPUT */

void elog_async_output(uint8_t level, const char *log, size_t size) {
    /* this function must be implement by user when ELOG_ASYNC_OUTPUT_USING_PTHREAD is not defined */
    extern void elog_async_output_notice(void);
    size_t put_size;

    if (is_enabled) {
        if (level >= OUTPUT_LVL) {
            put_size = async_put_log(log, size);
            /* notify output log thread */
            if (put_size > 0) {
                elog_async_output_notice();
            }
        } else {
            elog_port_output(log, size);
        }
    } else {
        elog_port_output(log, size);
    }
}

void elog_async_output_notice(void) {
    osal_sem_give(s_output_notice);
}

static void async_output(void *arg) {
    (void)arg;
    size_t get_log_size = 0;
    static char poll_get_buf[ELOG_ASYNC_POLL_GET_LOG_BUF_SIZE];

    while (true) {
        /* waiting log */
        if (s_output_notice == NULL) {   /* 初始化失败保护,避免空取空转 */
            break;
        }
        osal_sem_take(s_output_notice, OSAL_WAIT_FOREVER);
        /* polling gets and outputs the log */
        while (true) {

#ifdef ELOG_ASYNC_LINE_OUTPUT
            get_log_size = elog_async_get_line_log(poll_get_buf, ELOG_ASYNC_POLL_GET_LOG_BUF_SIZE);
#else
            get_log_size = elog_async_get_log(poll_get_buf, ELOG_ASYNC_POLL_GET_LOG_BUF_SIZE);
#endif

            if (get_log_size) {
                elog_port_output(poll_get_buf, get_log_size);
            } else {
                break;
            }
        }
    }
}

/**
 * enable or disable asynchronous output mode
 * the log will be output directly when mode is disabled
 *
 * @param enabled true: enabled, false: disabled
 */
void elog_async_enabled(bool enabled) {
    is_enabled = enabled;
}

/**
 * asynchronous output mode initialize
 *
 * @return result
 */
ElogErrCode elog_async_init(void) {
    ElogErrCode result = ELOG_NO_ERR;

    if (init_ok) {
        return result;
    }

    /* 通知信号量:计数模式(max 取满),线程忙时多次通知合并为一次唤醒 */
    s_output_notice = osal_sem_create(0xFFFFFFFFu, 0);
    /* 输出线程:分离运行 + 低优先级(日志写出是低价值工作,环形缓冲负责吸收) */
    s_output_thread = osal_task_create(async_output, NULL,
                                       OSAL_PRIO_LOWEST, OSAL_TASK_STACK_DEFAULT);
    (void)s_output_thread;   /* 句柄仅用于持有,分离线程无需 join */

    init_ok = true;

    return result;
}

#endif /* ELOG_ASYNC_OUTPUT_ENABLE */
