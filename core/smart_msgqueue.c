#include "smart_msgqueue.h"
#include "smart_core.h"
#include <string.h>

/* 初始化消息队列 */
void smart_msgqueue_init(smart_msgqueue_t *queue, smart_msg_t *buffer, uint32_t capacity)
{
    if (!queue || !buffer || capacity == 0)
    {
        return;
    }
    
    smart_enter_critical();
    
    queue->buffer = buffer;
    queue->capacity = capacity;
    queue->count = 0;
    queue->head = 0;
    queue->tail = 0;
    queue->dropped = 0;
    
    /* 清空缓冲区 */
    memset(buffer, 0, sizeof(smart_msg_t) * capacity);
    
    smart_exit_critical();
}

/* 发送消息（非阻塞） */
smart_msgq_status_t smart_msgqueue_send(smart_msgqueue_t *queue, const smart_msg_t *msg)
{
    if (!queue || !msg)
    {
        return SMART_MSGQ_INVALID;
    }
    
    smart_enter_critical();
    
    /* 检查队列是否已满 */
    if (queue->count >= queue->capacity)
    {
        queue->dropped++;
        smart_exit_critical();
        return SMART_MSGQ_FULL;
    }
    
    /* 复制消息到队尾 */
    queue->buffer[queue->tail] = *msg;
    
    /* 更新队尾指针（环形缓冲区） */
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;
    
    smart_exit_critical();
    
    return SMART_MSGQ_OK;
}

/* 接收消息（非阻塞） */
smart_msgq_status_t smart_msgqueue_receive(smart_msgqueue_t *queue, smart_msg_t *msg)
{
    if (!queue || !msg)
    {
        return SMART_MSGQ_INVALID;
    }
    
    smart_enter_critical();
    
    /* 检查队列是否为空 */
    if (queue->count == 0)
    {
        smart_exit_critical();
        return SMART_MSGQ_EMPTY;
    }
    
    /* 从队头取出消息 */
    *msg = queue->buffer[queue->head];
    
    /* 更新队头指针（环形缓冲区） */
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    
    smart_exit_critical();
    
    return SMART_MSGQ_OK;
}

/* 查询队列中的消息数量 */
uint32_t smart_msgqueue_count(const smart_msgqueue_t *queue)
{
    if (!queue)
    {
        return 0;
    }
    
    return queue->count;
}

/* 查询队列剩余空间 */
uint32_t smart_msgqueue_space(const smart_msgqueue_t *queue)
{
    if (!queue)
    {
        return 0;
    }
    
    return queue->capacity - queue->count;
}

/* 检查队列是否为空 */
int smart_msgqueue_is_empty(const smart_msgqueue_t *queue)
{
    if (!queue)
    {
        return 1;
    }
    
    return (queue->count == 0);
}

/* 检查队列是否已满 */
int smart_msgqueue_is_full(const smart_msgqueue_t *queue)
{
    if (!queue)
    {
        return 1;
    }
    
    return (queue->count >= queue->capacity);
}
