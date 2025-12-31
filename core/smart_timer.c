#include "smart_timer.h"
#include "smart_core.h"
#include "smart_uart.h"
#include <string.h>

/* 最大定时器数量 */
#define MAX_TIMERS 16

/* 定时器池 */
static smart_timer_t timer_pool[MAX_TIMERS];
static int timer_pool_init = 0;

/* 活跃定时器链表 */
static smart_timer_t *active_timer_list = NULL;

/* 定时器ID计数器 */
static uint32_t next_timer_id = 1;

/* 统计信息 */
static timer_stats_t timer_stats = {0};

/* 时间测量 */
static uint32_t get_microseconds(void)
{
    /* 简单的微秒计数器，基于系统滴答 */
    return smart_get_tick() * 1000;  /* 假设1ms滴答 */
}

/* 初始化定时器系统 */
void smart_timer_init(void)
{
    if (timer_pool_init)
    {
        return;
    }
    
    /* 初始化定时器池 */
    memset(timer_pool, 0, sizeof(timer_pool));
    active_timer_list = NULL;
    next_timer_id = 1;
    memset(&timer_stats, 0, sizeof(timer_stats));
    
    timer_pool_init = 1;
    
    smart_uart_print("[TIMER] Software timer system initialized\r\n");
}

/* 从池中分配定时器 */
static smart_timer_t* allocate_timer(void)
{
    smart_enter_critical();
    
    for (int i = 0; i < MAX_TIMERS; i++)
    {
        if (timer_pool[i].id == 0)  /* 未使用 */
        {
            timer_pool[i].id = next_timer_id++;
            timer_stats.total_timers++;
            smart_exit_critical();
            return &timer_pool[i];
        }
    }
    
    smart_exit_critical();
    return NULL;  /* 池已满 */
}

/* 释放定时器到池 */
static void free_timer(smart_timer_t *timer)
{
    if (!timer) return;
    
    smart_enter_critical();
    
    /* 从活跃链表中移除 */
    if (active_timer_list == timer)
    {
        active_timer_list = timer->next;
    }
    else
    {
        smart_timer_t *prev = active_timer_list;
        while (prev && prev->next != timer)
        {
            prev = prev->next;
        }
        if (prev)
        {
            prev->next = timer->next;
        }
    }
    
    /* 清零定时器 */
    memset(timer, 0, sizeof(smart_timer_t));
    timer_stats.total_timers--;
    
    smart_exit_critical();
}

/* 创建定时器 */
timer_handle_t smart_timer_create(timer_type_t type, uint32_t period_ms, 
                                  timer_callback_t callback, void *arg)
{
    if (!callback || period_ms == 0)
    {
        return NULL;
    }
    
    smart_timer_t *timer = allocate_timer();
    if (!timer)
    {
        return NULL;
    }
    
    timer->type = type;
    timer->state = TIMER_STOPPED;
    timer->period_ms = period_ms;
    timer->remaining_ms = period_ms;
    timer->callback = callback;
    timer->callback_arg = arg;
    timer->next = NULL;
    
    return timer;
}

/* 启动定时器 */
int smart_timer_start(timer_handle_t timer)
{
    if (!timer || timer->id == 0)
    {
        return -1;
    }
    
    smart_enter_critical();
    
    /* 如果已经在活跃链表中，先移除 */
    if (timer->state == TIMER_RUNNING)
    {
        smart_exit_critical();
        return 0;  /* 已经在运行 */
    }
    
    /* 重置剩余时间 */
    timer->remaining_ms = timer->period_ms;
    timer->state = TIMER_RUNNING;
    
    /* 添加到活跃链表头部 */
    timer->next = active_timer_list;
    active_timer_list = timer;
    timer_stats.active_timers++;
    
    smart_exit_critical();
    
    return 0;
}

/* 停止定时器 */
int smart_timer_stop(timer_handle_t timer)
{
    if (!timer || timer->id == 0)
    {
        return -1;
    }
    
    smart_enter_critical();
    
    if (timer->state != TIMER_RUNNING)
    {
        smart_exit_critical();
        return 0;  /* 已经停止 */
    }
    
    /* 从活跃链表中移除 */
    if (active_timer_list == timer)
    {
        active_timer_list = timer->next;
    }
    else
    {
        smart_timer_t *prev = active_timer_list;
        while (prev && prev->next != timer)
        {
            prev = prev->next;
        }
        if (prev)
        {
            prev->next = timer->next;
        }
    }
    
    timer->state = TIMER_STOPPED;
    timer->next = NULL;
    timer_stats.active_timers--;
    
    smart_exit_critical();
    
    return 0;
}

/* 重置定时器 */
int smart_timer_reset(timer_handle_t timer)
{
    if (!timer || timer->id == 0)
    {
        return -1;
    }
    
    smart_enter_critical();
    timer->remaining_ms = timer->period_ms;
    timer->state = TIMER_STOPPED;
    smart_exit_critical();
    
    return 0;
}

/* 删除定时器 */
int smart_timer_delete(timer_handle_t timer)
{
    if (!timer || timer->id == 0)
    {
        return -1;
    }
    
    /* 先停止定时器 */
    smart_timer_stop(timer);
    
    /* 释放定时器 */
    free_timer(timer);
    
    return 0;
}

/* 修改定时器周期 */
int smart_timer_set_period(timer_handle_t timer, uint32_t period_ms)
{
    if (!timer || timer->id == 0 || period_ms == 0)
    {
        return -1;
    }
    
    smart_enter_critical();
    timer->period_ms = period_ms;
    if (timer->state == TIMER_STOPPED)
    {
        timer->remaining_ms = period_ms;
    }
    smart_exit_critical();
    
    return 0;
}

/* 获取定时器剩余时间 */
uint32_t smart_timer_get_remaining(timer_handle_t timer)
{
    if (!timer || timer->id == 0)
    {
        return 0;
    }
    
    return timer->remaining_ms;
}

/* 获取定时器状态 */
timer_state_t smart_timer_get_state(timer_handle_t timer)
{
    if (!timer || timer->id == 0)
    {
        return TIMER_STOPPED;
    }
    
    return timer->state;
}

/* 获取统计信息 */
void smart_timer_get_stats(timer_stats_t *stats)
{
    if (stats)
    {
        smart_enter_critical();
        *stats = timer_stats;
        smart_exit_critical();
    }
}

/* 定时器系统滴答处理 */
void smart_timer_tick(void)
{
    if (!timer_pool_init)
    {
        return;
    }
    
    smart_enter_critical();
    
    smart_timer_t *timer = active_timer_list;
    smart_timer_t *prev = NULL;
    
    while (timer)
    {
        smart_timer_t *next = timer->next;
        
        if (timer->remaining_ms > 0)
        {
            timer->remaining_ms--;
        }
        
        /* 定时器到期 */
        if (timer->remaining_ms == 0)
        {
            timer->state = TIMER_EXPIRED;
            timer_stats.expired_count++;
            
            /* 从活跃链表中移除 */
            if (prev)
            {
                prev->next = next;
            }
            else
            {
                active_timer_list = next;
            }
            timer_stats.active_timers--;
            
            /* 执行回调函数 */
            if (timer->callback)
            {
                smart_exit_critical();  /* 执行回调时允许中断 */
                
                uint32_t start_time = get_microseconds();
                timer->callback(timer->callback_arg);
                uint32_t end_time = get_microseconds();
                
                uint32_t callback_time = end_time - start_time;
                if (callback_time > timer_stats.max_callback_time_us)
                {
                    timer_stats.max_callback_time_us = callback_time;
                }
                timer_stats.callback_count++;
                
                smart_enter_critical();
            }
            
            /* 处理周期定时器 */
            if (timer->type == TIMER_PERIODIC)
            {
                timer->remaining_ms = timer->period_ms;
                timer->state = TIMER_RUNNING;
                
                /* 重新添加到活跃链表 */
                timer->next = active_timer_list;
                active_timer_list = timer;
                timer_stats.active_timers++;
            }
            else
            {
                /* 单次定时器，保持EXPIRED状态 */
                timer->next = NULL;
            }
        }
        else
        {
            prev = timer;
        }
        
        timer = next;
    }
    
    smart_exit_critical();
}

/* 列出所有定时器 */
void smart_timer_list(void)
{
    smart_uart_print("=== Timer List ===\r\n");
    smart_uart_print("ID   Type      State     Period(ms) Remaining(ms)\r\n");
    smart_uart_print("------------------------------------------------\r\n");
    
    smart_enter_critical();
    
    for (int i = 0; i < MAX_TIMERS; i++)
    {
        if (timer_pool[i].id != 0)
        {
            smart_timer_t *timer = &timer_pool[i];
            
            smart_uart_print("0x");
            smart_uart_print_hex32(timer->id);
            smart_uart_print(" ");
            
            if (timer->type == TIMER_ONE_SHOT)
            {
                smart_uart_print("OneShot   ");
            }
            else
            {
                smart_uart_print("Periodic  ");
            }
            
            switch (timer->state)
            {
                case TIMER_STOPPED:
                    smart_uart_print("Stopped   ");
                    break;
                case TIMER_RUNNING:
                    smart_uart_print("Running   ");
                    break;
                case TIMER_EXPIRED:
                    smart_uart_print("Expired   ");
                    break;
            }
            
            smart_uart_print("0x");
            smart_uart_print_hex32(timer->period_ms);
            smart_uart_print("     0x");
            smart_uart_print_hex32(timer->remaining_ms);
            smart_uart_print("\r\n");
        }
    }
    
    smart_exit_critical();
    
    smart_uart_print("\r\n=== Timer Statistics ===\r\n");
    smart_uart_print("Total Timers: 0x");
    smart_uart_print_hex32(timer_stats.total_timers);
    smart_uart_print("\r\nActive Timers: 0x");
    smart_uart_print_hex32(timer_stats.active_timers);
    smart_uart_print("\r\nExpired Count: 0x");
    smart_uart_print_hex32(timer_stats.expired_count);
    smart_uart_print("\r\nCallback Count: 0x");
    smart_uart_print_hex32(timer_stats.callback_count);
    smart_uart_print("\r\nMax Callback Time: 0x");
    smart_uart_print_hex32(timer_stats.max_callback_time_us);
    smart_uart_print(" us\r\n");
}