#ifndef __SMART_MSGQUEUE_H__
#define __SMART_MSGQUEUE_H__

#include <stdint.h>
#include <stddef.h>

/* 消息队列状态 */
typedef enum
{
    SMART_MSGQ_OK = 0,
    SMART_MSGQ_FULL,
    SMART_MSGQ_EMPTY,
    SMART_MSGQ_INVALID
} smart_msgq_status_t;

/* 消息结构 */
typedef struct
{
    uint32_t type;      /* 消息类型 */
    uint32_t data;      /* 消息数据 */
    void *ptr;          /* 可选指针 */
} smart_msg_t;

/* 消息队列控制块 */
typedef struct
{
    smart_msg_t *buffer;    /* 消息缓冲区 */
    uint32_t capacity;      /* 队列容量 */
    uint32_t count;         /* 当前消息数 */
    uint32_t head;          /* 队头索引 */
    uint32_t tail;          /* 队尾索引 */
    uint32_t dropped;       /* 丢弃消息计数 */
} smart_msgqueue_t;

/* 初始化消息队列 */
void smart_msgqueue_init(smart_msgqueue_t *queue, smart_msg_t *buffer, uint32_t capacity);

/* 发送消息（非阻塞） */
smart_msgq_status_t smart_msgqueue_send(smart_msgqueue_t *queue, const smart_msg_t *msg);

/* 接收消息（非阻塞） */
smart_msgq_status_t smart_msgqueue_receive(smart_msgqueue_t *queue, smart_msg_t *msg);

/* 查询队列状态 */
uint32_t smart_msgqueue_count(const smart_msgqueue_t *queue);
uint32_t smart_msgqueue_space(const smart_msgqueue_t *queue);
int smart_msgqueue_is_empty(const smart_msgqueue_t *queue);
int smart_msgqueue_is_full(const smart_msgqueue_t *queue);

#endif
