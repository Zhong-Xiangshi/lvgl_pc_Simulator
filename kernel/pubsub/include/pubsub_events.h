#ifndef PUBSUB_EVENTS_H
#define PUBSUB_EVENTS_H

/**
 * =====================================================================
 * pubsub 公共事件定义(话题注册表)
 *
 * 全系统共享的"话题名 + 载荷结构体"清单:发布方与订阅方都引用本文件的
 * 宏与结构体,保证话题字符串与载荷布局唯一、一致(不裸写字符串、不各
 * 自定义结构)。需要时 #include "pubsub_events.h"(与 pubsub.h 同目录,
 * pubsub.h 不自动包含本文件,由使用方按需引入)。
 *
 * 登记格式:新增话题 = 一行话题宏(字符串,建议"域/名"两级前缀)+
 * 成对载荷结构体,按如下分节注释登记。载荷经 pubsub_*_publish 零拷贝
 * 传递,订阅者回调内只读;结构体内的指针字段存活期与 payload 一致
 * (同步发布=发布者阻塞期内,异步发布=broker fan-out 时刻),需要长期
 * 数据须由接收方自持内存。
 * =====================================================================
 */

#include <stdint.h>

/* ===== demo/tick:周期节拍事件 ===== */
#define PUBSUB_TOPIC_DEMO_TICK "demo/tick"
typedef struct {
    uint32_t seq;     /* 序号 */
    uint32_t ms;      /* 距上一拍的毫秒数 */
} pubsub_tick_evt_t;

/* ===== demo/msg:演示消息事件 ===== */
#define PUBSUB_TOPIC_DEMO_MSG "demo/msg"
typedef struct {
    uint32_t   code;
    const char *text;   /* 存活期与 payload 一致:仅回调执行期间有效 */
} pubsub_msg_evt_t;

#endif /* PUBSUB_EVENTS_H */
