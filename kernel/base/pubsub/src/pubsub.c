/**
 * =====================================================================
 * pubsub 发布-订阅消息总线实现(单例 broker)
 *
 * 线程模型:所有对全局主题表 / 订阅表的读写只发生在 broker 线程内——
 * 订阅、退订、发布都经 ao_post_async / ao_call_sync 委托成队列消息,由
 * broker 在 ao_process 里串行执行(队列 FIFO 保证全序),因此本文件无需
 * 任何锁;其他线程只"投递消息指针",不触碰共享表。
 *
 * 内存所有权:
 *   - 主题节点:首个订阅者到来时创建(strdup 键 + 节点),最后一名退订时
 *     随表项一起释放;
 *   - 订阅节点(句柄本体):subscribe 分配,unsubscribe(同步/异步)释放;
 *   - 同步信封:调用者栈上,ao_call_sync 阻塞期间由 broker 线程回写;
 *   - 异步发布信封:单块 osal_malloc(主题名副本内嵌尾部),fan-out 后由
 *     ao_process 的 free_fn 统一收尾(含 payload 释放)。
 * =====================================================================
 */

#include "pubsub.h"

#include <string.h>   /* strlen / memcpy */

#include "osal.h"
#include "ao.h"
#include "uthash.h"   /* 宏库;内部分配已适配 osal_malloc/osal_free */

/* ==================== 数据结构 ==================== */
/* 订阅节点(即公开的 pubsub_sub_t 句柄本体,头文件已前向声明其 tag) */
struct pubsub_sub {
    uint32_t          id;       /* 主题内唯一,全局递增分配(2^32 回绕撞键概率可忽略) */
    pubsub_handler_t  handler;
    void             *ctx;
    struct pubsub_topic *topic; /* 反指所属主题节点,退订时拿表头(仅 broker 线程访问) */
    UT_hash_handle    hh;       /* 挂在 topic->subs 上,key=id */
};

struct pubsub_topic {
    char            *name;      /* uthash 键:建节点时 osal_malloc 复制,须持久到节点删除 */
    pubsub_sub_t    *subs;      /* 本主题订阅者表头(key=sub->id) */
    UT_hash_handle   hh;        /* 挂在全局 s_topics 上,key=name */
};

/* 同步订阅信封:调用者栈上,do_subscribe(broker 线程)回写 out */
typedef struct {
    const char       *topic;
    pubsub_handler_t  handler;
    void             *ctx;
    pubsub_sub_t     *out;      /* 回填句柄;NULL=失败 */
} pubsub_sub_req_t;

/* 同步发布信封:调用者栈上,do_publish(broker 线程)回写 delivered */
typedef struct {
    const char *topic;
    void       *payload;
    uint32_t    delivered;      /* 回填送达数 */
} pubsub_pub_req_t;

/* 异步发布信封:单块 osal_malloc,主题名副本内嵌尾部(柔性数组) */
typedef struct {
    void             *payload;
    pubsub_free_fn_t  payload_free;  /* 可为 NULL(静态 payload) */
    char              topic[];       /* 主题名副本(含 NUL),发布时复制 */
} pubsub_async_msg_t;

/* ==================== 单例状态(file-scope;表只在 broker 线程访问) ==================== */
static ao_t            s_ao;
static struct pubsub_topic *s_topics = NULL;   /* 全局主题表头(uthash) */
static uint32_t        s_next_id = 1u;         /* 订阅 id 分配器 */
static int             s_inited  = 0;          /* 幂等标志(首调须单线程) */

/* ==================== 内部工具 ==================== */
/* pubsub 专用 strdup:一律走 osal_malloc+memcpy(移植时无需改库代码) */
static char *pubsub_strdup(const char *s)
{
    size_t n = strlen(s) + 1u;
    char  *p = osal_malloc(n);
    if (p != NULL) memcpy(p, s, n);
    return p;
}

/* ==================== broker 线程 ==================== */
static void pubsub_broker_entry(void *arg)
{
    (void)arg;
    /* 纯事件驱动、无周期性家务:无限阻塞等消息(有消息时 queue_send 的
     * cond_signal 即时唤醒),ao_process 仅在队列无效时返回 -1(本组件
     * 从不 deinit,不会发生) */
    while (1) {
        ao_process(&s_ao, OSAL_WAIT_FOREVER);
    }
}

/* ==================== fan-out 公共核心(仅在 broker 线程执行) ==================== */
static uint32_t pubsub_fanout(const char *topic, void *payload)
{
    struct pubsub_topic *t = NULL;
    HASH_FIND_STR(s_topics, topic, t);          /* 精确字符串匹配,无通配符 */
    if (t == NULL) return 0u;

    pubsub_sub_t *sub = NULL, *tmp = NULL;
    uint32_t n = 0u;
    /* HASH_ITER 迭代中不删节点:回调发起的退订请求排在本条消息之后执行,
     * 当前 fan-out 期间表结构不会变化,天然安全 */
    HASH_ITER(hh, t->subs, sub, tmp) {
        sub->handler(topic, payload, sub->ctx);
        n++;
    }
    return n;
}

/* ==================== dispatch 函数(均被 ao 投递,在 broker 线程执行) ==================== */

/* 订阅:主题不存在则创建节点(键 strdup 持久化);订阅节点分配失败时
 * 回滚刚建的主题节点,避免孤儿表项 */
static void pubsub_do_subscribe(void *data)
{
    pubsub_sub_req_t *req = data;
    req->out = NULL;

    struct pubsub_topic *t = NULL;
    HASH_FIND_STR(s_topics, req->topic, t);
    int created = 0;
    if (t == NULL) {
        char *name = pubsub_strdup(req->topic);
        if (name == NULL) return;
        t = osal_malloc(sizeof(*t));
        if (t == NULL) { osal_free(name); return; }
        t->name = name;
        t->subs = NULL;                         /* osal_malloc 不清零,逐字段初始化 */
        /* keylen 必须传 strlen(不含 NUL):HASH_FIND_STR 内部按 strlen 算长度 */
        HASH_ADD_KEYPTR(hh, s_topics, t->name, strlen(t->name), t);
        created = 1;
    }

    pubsub_sub_t *sub = osal_malloc(sizeof(*sub));
    if (sub == NULL) {
        if (created) {                          /* 回滚空主题节点 */
            HASH_DEL(s_topics, t);
            osal_free(t->name);
            osal_free(t);
        }
        return;
    }
    sub->id      = s_next_id++;                 /* HASH_ADD_INT 宏内读 sub->id,须先赋值 */
    sub->handler = req->handler;
    sub->ctx     = req->ctx;
    sub->topic   = t;
    sub->hh      = (UT_hash_handle){0};         /* osal_malloc 不清零,显式清 hh */
    HASH_ADD_INT(t->subs, id, sub);
    req->out = sub;
}

/* 退订:data 即订阅句柄;摘除并释放节点,最后一名订阅者退订时连主题
 * 节点与主题名副本一并释放 */
static void pubsub_do_unsubscribe(void *data)
{
    pubsub_sub_t     *sub = data;
    struct pubsub_topic *t = sub->topic;

    HASH_DEL(t->subs, sub);
    osal_free(sub);
    if (t->subs == NULL) {                      /* 主题已无订阅者 */
        HASH_DEL(s_topics, t);
        osal_free(t->name);
        osal_free(t);
    }
}

/* 同步发布:只做 fan-out,送达数回写调用者栈上信封 */
static void pubsub_do_publish(void *data)
{
    pubsub_pub_req_t *req = data;
    req->delivered = pubsub_fanout(req->topic, req->payload);
}

/* 异步发布:只做 fan-out;信封与 payload 的内存由 ao_process 的 free_fn 收尾 */
static void pubsub_do_publish_async(void *data)
{
    pubsub_async_msg_t *m = data;
    (void)pubsub_fanout(m->topic, m->payload);
}

/* 异步信封释放器:先释放 payload(如带回调),再释放信封本体。
 * 两个调用路径只会发生其一且只一次:
 *   正常:ao_process 在 fn 执行完后调用(broker 线程);
 *   投递失败:ao_post_async 在调用线程内立即调用(见 ao.h)。 */
static void pubsub_async_msg_free(void *data)
{
    pubsub_async_msg_t *m = data;
    if (m->payload_free != NULL) m->payload_free(m->payload);
    osal_free(m);
}

/* ==================== 公开 API ==================== */
int pubsub_init(void)
{
    if (s_inited) return 0;
    if (ao_init(&s_ao, PUBSUB_QUEUE_LEN) != 0) return -1;
    if (osal_task_create(pubsub_broker_entry, NULL, OSAL_PRIO_NORMAL,
                         PUBSUB_TASK_STACK_BYTES) == NULL) {
        ao_deinit(&s_ao);
        return -1;
    }
    s_inited = 1;
    return 0;
}

pubsub_sub_t *pubsub_subscribe(const char *topic, pubsub_handler_t handler, void *ctx)
{
    if (!s_inited || topic == NULL || topic[0] == '\0' || handler == NULL) {
        PUBSUB_LOG("pubsub_subscribe: 未初始化或参数错误\n");
        return NULL;
    }
    pubsub_sub_req_t req = { .topic = topic, .handler = handler, .ctx = ctx, .out = NULL };
    if (ao_call_sync(&s_ao, pubsub_do_subscribe, &req) != 0) return NULL;
    return req.out;                             /* 信封在调用者栈上,do_* 已回写 */
}

int pubsub_unsubscribe(pubsub_sub_t *sub)
{
    if (!s_inited || sub == NULL) return -1;
    return ao_call_sync(&s_ao, pubsub_do_unsubscribe, sub);   /* 0=成功 */
}

int pubsub_unsubscribe_async(pubsub_sub_t *sub)
{
    if (!s_inited || sub == NULL) return -1;
    /* data=句柄本身即消息:句柄存活到该消息被消费(do_unsubscribe 释放),
     * 排队期间指针必然有效;free_fn 传 NULL——投递失败时 ao_post_async
     * 不回收,句柄仍在表中,调用者可重试 */
    return ao_post_async(&s_ao, pubsub_do_unsubscribe, sub, NULL);
}

int pubsub_publish_sync(const char *topic, void *payload)
{
    if (!s_inited || topic == NULL || topic[0] == '\0') return -1;
    pubsub_pub_req_t req = { .topic = topic, .payload = payload, .delivered = 0u };
    if (ao_call_sync(&s_ao, pubsub_do_publish, &req) != 0) return -1;
    return (int)req.delivered;                  /* 送达数;无订阅者为 0 */
}

int pubsub_publish_async(const char *topic, void *payload, pubsub_free_fn_t payload_free)
{
    /* 统一内存契约:调用后 payload 所有权无条件转移,任一失败路径都负责
     * 释放 payload(勿让调用方陷入"有的失败要自己释放、有的不能"的歧义) */
    if (!s_inited || topic == NULL || topic[0] == '\0') {
        PUBSUB_LOG("pubsub_publish_async: 未初始化或参数错误,payload 已释放\n");
        if (payload_free != NULL) payload_free(payload);
        return -1;
    }
    size_t tlen = strlen(topic);
    pubsub_async_msg_t *m = osal_malloc(sizeof(*m) + tlen + 1u);
    if (m == NULL) {
        if (payload_free != NULL) payload_free(payload);   /* 失败即释放 */
        return -1;
    }
    m->payload      = payload;
    m->payload_free = payload_free;
    memcpy(m->topic, topic, tlen + 1u);         /* 此刻复制,调用者返回后仍安全 */
    /* 队列满时 ao_post_async 已代为调用 pubsub_async_msg_free(m),契约闭合 */
    return ao_post_async(&s_ao, pubsub_do_publish_async, m, pubsub_async_msg_free);
}
