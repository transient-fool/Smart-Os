#ifndef __SMART_SYNC_H__
#define __SMART_SYNC_H__

#include <stdint.h>
#include "smart_core.h"

/* 同步状态 */
typedef enum {
    SMART_SYNC_OK = 0,
    SMART_SYNC_TIMEOUT,
    SMART_SYNC_ERROR
} smart_sync_status_t;

/* ========== 信号量 ========== */

typedef struct {
    uint32_t count;           /* 当前计数值 */
    uint32_t max_count;       /* 最大计数值 */
    smart_task_t wait_list;   /* 等待队列 */
} smart_semaphore_t;

/* 初始化信号量 */
void smart_sem_init(smart_semaphore_t *sem, uint32_t initial_count, uint32_t max_count);

/* 获取信号量（阻塞） */
smart_sync_status_t smart_sem_wait(smart_semaphore_t *sem);

/* 获取信号量（超时） */
smart_sync_status_t smart_sem_wait_timeout(smart_semaphore_t *sem, uint32_t timeout_ms);

/* 获取信号量（非阻塞） */
smart_sync_status_t smart_sem_try_wait(smart_semaphore_t *sem);

/* 释放信号量 */
smart_sync_status_t smart_sem_post(smart_semaphore_t *sem);

/* 获取信号量计数 */
uint32_t smart_sem_get_count(smart_semaphore_t *sem);

/* ========== 互斥锁 ========== */

typedef struct {
    uint8_t locked;           /* 锁状态：0=未锁，1=已锁 */
    smart_task_t owner;       /* 持有锁的任务 */
    uint32_t lock_count;      /* 递归锁计数 */
    uint32_t original_deadline; /* 原始deadline（优先级继承用） */
    smart_task_t wait_list;   /* 等待队列 */
} smart_mutex_t;

/* 初始化互斥锁 */
void smart_mutex_init(smart_mutex_t *mutex);

/* 获取互斥锁（阻塞） */
smart_sync_status_t smart_mutex_lock(smart_mutex_t *mutex);

/* 获取互斥锁（超时） */
smart_sync_status_t smart_mutex_lock_timeout(smart_mutex_t *mutex, uint32_t timeout_ms);

/* 获取互斥锁（非阻塞） */
smart_sync_status_t smart_mutex_try_lock(smart_mutex_t *mutex);

/* 释放互斥锁 */
smart_sync_status_t smart_mutex_unlock(smart_mutex_t *mutex);

/* 检查是否持有锁 */
int smart_mutex_is_locked(smart_mutex_t *mutex);

#endif
