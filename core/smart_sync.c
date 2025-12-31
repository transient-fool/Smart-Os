#include "smart_sync.h"
#include "smart_core.h"
#include <string.h>

/* ========== 信号量实现 ========== */

void smart_sem_init(smart_semaphore_t *sem, uint32_t initial_count, uint32_t max_count)
{
    if (!sem || max_count == 0)
    {
        return;
    }
    
    smart_enter_critical();
    
    sem->count = initial_count > max_count ? max_count : initial_count;
    sem->max_count = max_count;
    sem->wait_list = NULL;
    
    smart_exit_critical();
}

smart_sync_status_t smart_sem_wait(smart_semaphore_t *sem)
{
    if (!sem)
    {
        return SMART_SYNC_ERROR;
    }
    
    smart_enter_critical();
    
    /* 如果有可用信号量，直接获取 */
    if (sem->count > 0)
    {
        sem->count--;
        smart_exit_critical();
        return SMART_SYNC_OK;
    }
    
    /* 否则，将当前任务加入等待队列 */
    smart_task_t current = smart_get_current_task();
    if (current)
    {
        current->state = TASK_STATE_WAITING;
        
        /* 添加到等待队列 */
        if (sem->wait_list == NULL)
        {
            sem->wait_list = current;
            current->next = NULL;
        }
        else
        {
            /* 插入到队列末尾 */
            smart_task_t node = sem->wait_list;
            while (node->next != NULL)
            {
                node = node->next;
            }
            node->next = current;
            current->next = NULL;
        }
    }
    
    smart_exit_critical();
    
    /* 触发调度，切换到其他任务 */
    smart_task_yield();
    
    return SMART_SYNC_OK;
}

smart_sync_status_t smart_sem_wait_timeout(smart_semaphore_t *sem, uint32_t timeout_ms)
{
    if (!sem)
    {
        return SMART_SYNC_ERROR;
    }
    
    uint32_t start_time = smart_get_tick();
    
    while (1)
    {
        /* 尝试获取信号量 */
        if (smart_sem_try_wait(sem) == SMART_SYNC_OK)
        {
            return SMART_SYNC_OK;
        }
        
        /* 检查超时 */
        if ((smart_get_tick() - start_time) >= timeout_ms)
        {
            return SMART_SYNC_TIMEOUT;
        }
        
        /* 让出CPU */
        smart_task_yield();
    }
}

smart_sync_status_t smart_sem_try_wait(smart_semaphore_t *sem)
{
    if (!sem)
    {
        return SMART_SYNC_ERROR;
    }
    
    smart_enter_critical();
    
    if (sem->count > 0)
    {
        sem->count--;
        smart_exit_critical();
        return SMART_SYNC_OK;
    }
    
    smart_exit_critical();
    return SMART_SYNC_TIMEOUT;
}

smart_sync_status_t smart_sem_post(smart_semaphore_t *sem)
{
    if (!sem)
    {
        return SMART_SYNC_ERROR;
    }
    
    smart_enter_critical();
    
    /* 如果有等待的任务，唤醒第一个 */
    if (sem->wait_list != NULL)
    {
        smart_task_t task = sem->wait_list;
        sem->wait_list = task->next;
        task->next = NULL;
        task->state = TASK_STATE_READY;
        
        smart_exit_critical();
        
        /* 触发调度 */
        smart_schedule();
        
        return SMART_SYNC_OK;
    }
    
    /* 否则，增加计数 */
    if (sem->count < sem->max_count)
    {
        sem->count++;
    }
    
    smart_exit_critical();
    
    return SMART_SYNC_OK;
}

uint32_t smart_sem_get_count(smart_semaphore_t *sem)
{
    if (!sem)
    {
        return 0;
    }
    
    return sem->count;
}

/* ========== 互斥锁实现 ========== */

void smart_mutex_init(smart_mutex_t *mutex)
{
    if (!mutex)
    {
        return;
    }
    
    smart_enter_critical();
    
    mutex->locked = 0;
    mutex->owner = NULL;
    mutex->lock_count = 0;
    mutex->original_deadline = 0;
    mutex->wait_list = NULL;
    
    smart_exit_critical();
}

smart_sync_status_t smart_mutex_lock(smart_mutex_t *mutex)
{
    if (!mutex)
    {
        return SMART_SYNC_ERROR;
    }
    
    smart_task_t current = smart_get_current_task();
    if (!current)
    {
        return SMART_SYNC_ERROR;
    }
    
    smart_enter_critical();
    
    /* 如果锁未被占用，直接获取 */
    if (!mutex->locked)
    {
        mutex->locked = 1;
        mutex->owner = current;
        mutex->lock_count = 1;
        mutex->original_deadline = current->deadline;
        
        smart_exit_critical();
        return SMART_SYNC_OK;
    }
    
    /* 如果是同一个任务（递归锁） */
    if (mutex->owner == current)
    {
        mutex->lock_count++;
        smart_exit_critical();
        return SMART_SYNC_OK;
    }
    
    /* 优先级继承：如果当前任务优先级更高，提升锁持有者的优先级 */
    if (current->deadline < mutex->owner->deadline)
    {
        mutex->owner->deadline = current->deadline;
    }
    
    /* 将当前任务加入等待队列 */
    current->state = TASK_STATE_WAITING;
    
    if (mutex->wait_list == NULL)
    {
        mutex->wait_list = current;
        current->next = NULL;
    }
    else
    {
        smart_task_t node = mutex->wait_list;
        while (node->next != NULL)
        {
            node = node->next;
        }
        node->next = current;
        current->next = NULL;
    }
    
    smart_exit_critical();
    
    /* 触发调度 */
    smart_task_yield();
    
    return SMART_SYNC_OK;
}

smart_sync_status_t smart_mutex_lock_timeout(smart_mutex_t *mutex, uint32_t timeout_ms)
{
    if (!mutex)
    {
        return SMART_SYNC_ERROR;
    }
    
    uint32_t start_time = smart_get_tick();
    
    while (1)
    {
        /* 尝试获取锁 */
        if (smart_mutex_try_lock(mutex) == SMART_SYNC_OK)
        {
            return SMART_SYNC_OK;
        }
        
        /* 检查超时 */
        if ((smart_get_tick() - start_time) >= timeout_ms)
        {
            return SMART_SYNC_TIMEOUT;
        }
        
        /* 让出CPU */
        smart_task_yield();
    }
}

smart_sync_status_t smart_mutex_try_lock(smart_mutex_t *mutex)
{
    if (!mutex)
    {
        return SMART_SYNC_ERROR;
    }
    
    smart_task_t current = smart_get_current_task();
    if (!current)
    {
        return SMART_SYNC_ERROR;
    }
    
    smart_enter_critical();
    
    /* 如果锁未被占用，直接获取 */
    if (!mutex->locked)
    {
        mutex->locked = 1;
        mutex->owner = current;
        mutex->lock_count = 1;
        mutex->original_deadline = current->deadline;
        
        smart_exit_critical();
        return SMART_SYNC_OK;
    }
    
    /* 如果是同一个任务（递归锁） */
    if (mutex->owner == current)
    {
        mutex->lock_count++;
        smart_exit_critical();
        return SMART_SYNC_OK;
    }
    
    smart_exit_critical();
    return SMART_SYNC_TIMEOUT;
}

smart_sync_status_t smart_mutex_unlock(smart_mutex_t *mutex)
{
    if (!mutex)
    {
        return SMART_SYNC_ERROR;
    }
    
    smart_task_t current = smart_get_current_task();
    if (!current)
    {
        return SMART_SYNC_ERROR;
    }
    
    smart_enter_critical();
    
    /* 检查是否是锁的持有者 */
    if (mutex->owner != current)
    {
        smart_exit_critical();
        return SMART_SYNC_ERROR;
    }
    
    /* 递归锁计数减1 */
    mutex->lock_count--;
    
    /* 如果还有递归锁，不释放 */
    if (mutex->lock_count > 0)
    {
        smart_exit_critical();
        return SMART_SYNC_OK;
    }
    
    /* 恢复原始优先级 */
    current->deadline = mutex->original_deadline;
    
    /* 如果有等待的任务，唤醒优先级最高的 */
    if (mutex->wait_list != NULL)
    {
        /* 找到deadline最小的任务 */
        smart_task_t best = mutex->wait_list;
        smart_task_t prev_best = NULL;
        smart_task_t prev = NULL;
        smart_task_t node = mutex->wait_list;
        
        while (node != NULL)
        {
            if (node->deadline < best->deadline)
            {
                prev_best = prev;
                best = node;
            }
            prev = node;
            node = node->next;
        }
        
        /* 从等待队列移除 */
        if (prev_best == NULL)
        {
            mutex->wait_list = best->next;
        }
        else
        {
            prev_best->next = best->next;
        }
        
        best->next = NULL;
        best->state = TASK_STATE_READY;
        
        /* 新的持有者 */
        mutex->owner = best;
        mutex->lock_count = 1;
        mutex->original_deadline = best->deadline;
        
        smart_exit_critical();
        
        /* 触发调度 */
        smart_schedule();
        
        return SMART_SYNC_OK;
    }
    
    /* 释放锁 */
    mutex->locked = 0;
    mutex->owner = NULL;
    
    smart_exit_critical();
    
    return SMART_SYNC_OK;
}

int smart_mutex_is_locked(smart_mutex_t *mutex)
{
    if (!mutex)
    {
        return 0;
    }
    
    return mutex->locked;
}
