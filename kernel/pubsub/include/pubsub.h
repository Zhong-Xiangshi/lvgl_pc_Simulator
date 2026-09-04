#ifndef PUBSUB_H
#define PUBSUB_H

/**
 * =====================================================================
 * pubsub 发布-订阅消息总线(单例 broker AO 线程,基于 uthash)
 *
 * 模块用途:进程内"主题→订阅者"事件总线。发布者与订阅者解耦,任何模块
 * 线程均可安全调用:所有请求(订阅/退订/发布)作为委托消息投递给 pubsub
 * 自建 AO 线程(kernel/ao)串行执行,发布支持同步/异步两种。
 *
 * 架构:
 *   - 单例 broker:pubsub.c 内 file-scope static ao_t + 自建线程
 *     (形态仿 app/lvgl_service.c);全局状态只被 broker 线程访问,无锁;
 *   - 两级 uthash:全局"主题名→主题节点",主题内"订阅 id→订阅节点",
 *     发布按主题查找 O(1),不需要遍历全表;
 *   - payload 零拷贝:只传指针;publish_async 的 payload 所有权转移给本
 *     组件(可带释放回调),任何失败路径也由本组件负责释放;
 *   - 默认话题与载荷结构体见 pubsub_events.h(公共事件注册表,可选包含)。
 *
 * 订阅者回调约束(必须遵守;本组件不做运行时线程身份检测,靠文档约束):
 *   - 回调在 broker 线程内被同步串行调用,fan-out 全部执行完才算发布
 *     完成,回调必须短小非阻塞;需要在自己线程执行的逻辑请自行转发
 *     (如经 lvgl_service_post_async 转投 LVGL 线程);
 *   - 回调内禁止调用任何同步 API(pubsub_subscribe / pubsub_unsubscribe /
 *     pubsub_publish_sync):broker 线程正被当前回调占据,回不到事件循环
 *     消费下一条消息,同步调用必然死锁;回调内只可调用 *_async 系列;
 *   - topic / payload 指针仅在回调执行期间有效,禁止保存;同主题回调
 *     执行顺序与订阅顺序无关(uthash 遍历序),回调间不得依赖先后;
 *   - 回调中异步退订自己:于当前 fan-out 完成之后生效,不打断本轮。
 *
 * 时序 / 失败语义:
 *   - 所有操作按入队顺序(FIFO)在 broker 线程执行;同步发布阻塞时长 =
 *     当前全部订阅者回调时长总和;
 *   - publish_async 调用后 payload 所有权无条件转移:无论返回值,调用方
 *     不得再使用或再次释放 payload(失败路径组件已自动释放);payload_free
 *     须 NULL 安全,且可能被调用线程(投递失败)或 broker 线程调用;
 *   - publish_sync 返回本次送达的订阅者数;队列积压投递超时(见 ao.h
 *     AO_SYNC_SEND_TIMEOUT_MS,默认 100 ms)返回 -1,本次发布丢弃;
 *   - 订阅句柄退订后失效,重复退订/使用属未定义行为(同 OSAL"删除使用
 *     中对象属 UB"约定);本组件不提供 deinit(进程级单例)。
 *
 * 移植注意:
 *   - 依赖 kernel/osal 与 kernel/ao;所有内存分配走 osal_malloc/osal_free;
 *   - uthash 内部扩容失败默认 exit(-1)(未开 HASH_NONFATAL_OOM),PC 模拟
 *     可接受;组件自身的分配失败均返回错误码并做回滚,不终止;
 *   - pubsub_init 幂等,但首次调用须在初始化阶段由单一线程完成,其余
 *     API 在 init 之前调用一律返回失败。
 * =====================================================================
 */

#include <stdio.h>  /* PUBSUB_LOG 默认用 printf */

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 可覆盖配置:包含本文件前 #define 同名宏可覆盖默认值 ==================== */
#ifndef PUBSUB_QUEUE_LEN
#define PUBSUB_QUEUE_LEN 16u            /* broker 消息队列容量 */
#endif
#ifndef PUBSUB_TASK_STACK_BYTES
#define PUBSUB_TASK_STACK_BYTES (8u*1024u)  /* broker 线程栈:订阅者回调运行在此栈,勿设过小 */
#endif
#ifndef PUBSUB_LOG
#define PUBSUB_LOG(...) printf(__VA_ARGS__)  /* 诊断打印,可覆盖为空 */
#endif

/* ==================== 类型 ==================== */
typedef struct pubsub_sub pubsub_sub_t;  /* 订阅句柄(不透明,退订后失效) */

/** @brief 订阅者回调,在 broker 线程内被串行调用,约束见文件头横幅 */
typedef void (*pubsub_handler_t)(const char *topic, void *payload, void *ctx);

/** @brief 异步发布 payload 的释放回调(与 osal_free 签名兼容,可直接传 osal_free) */
typedef void (*pubsub_free_fn_t)(void *payload);

/* ==================== 初始化 ==================== */
/** @brief 初始化总线并创建 broker 线程(幂等)。
 *  @return 0 成功;-1 队列或线程创建失败。
 *  @note 首次调用须在初始化阶段由单一线程完成 */
int pubsub_init(void);

/* ==================== 订阅 / 退订 ==================== */
/**
 * @brief 订阅主题(同步:阻塞至 broker 处理完)。
 * @param topic   主题名,精确字符串匹配,无通配符;须非空
 * @param handler 回调函数,必须非 NULL
 * @param ctx     用户上下文,回调原样带回,可为 NULL
 * @return 订阅句柄;参数错误/未 init/内存不足返回 NULL。
 *         同一主题可重复订阅(各自独立句柄,发布时各回调一次)
 */
pubsub_sub_t *pubsub_subscribe(const char *topic, pubsub_handler_t handler, void *ctx);

/**
 * @brief 同步退订:阻塞至 broker 处理完,之后句柄失效,勿再使用。
 *        禁止在订阅者回调内调用(死锁),回调内请用 pubsub_unsubscribe_async。
 * @return 0 成功;-1 参数错误/未 init/投递超时。
 * @note 重复退订同一句柄属未定义行为
 */
int pubsub_unsubscribe(pubsub_sub_t *sub);

/**
 * @brief 异步退订:仅入队即返回,于"当前正在执行的 fan-out 完成之后"
 *        生效,适合在订阅者回调内退订自己。投递失败时句柄仍在表中,
 *        可重试。
 * @return 0 成功;-1 参数错误/未 init/队列满
 */
int pubsub_unsubscribe_async(pubsub_sub_t *sub);

/* ==================== 发布 ==================== */
/**
 * @brief 同步发布:阻塞直到该主题所有订阅者回调执行完毕(回调在 broker
 *        线程内执行)。topic 字符串在调用期间须保持有效(字面量/栈上均可)。
 * @return 送达的订阅者数(无订阅者为 0);-1 参数错误/未 init/队列积压
 *         投递超时(本次发布丢弃,见 AO_SYNC_SEND_TIMEOUT_MS)
 */
int pubsub_publish_sync(const char *topic, void *payload);

/**
 * @brief 异步发布:payload 零拷贝,所有权转移给本组件,处理完由
 *        payload_free 释放(可为 NULL 表示静态/长期存活数据)。
 *        调用后无论返回值,payload 均不得再使用(失败路径已自动释放,
 *        勿重复释放)。
 * @return 0 已入队(待 broker 执行);-1 参数错误/未 init/队列满
 */
int pubsub_publish_async(const char *topic, void *payload, pubsub_free_fn_t payload_free);

#ifdef __cplusplus
}
#endif

#endif /* PUBSUB_H */
