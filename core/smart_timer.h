#ifndef __SMART_TIMER_H__
#define __SMART_TIMER_H__

#include <stdint.h>

/* 定时器类型 */
typedef enum {
    TIMER_ONE_SHOT = 0,    /* 单次定时器 */
    TIMER_PERIODIC = 1     /* 周期定时器 */
} timer_type_t;

/* 定时器状态 */
typedef enum {
    TIMER_STOPPED = 0,     /* 停止状态 */
    TIMER_RUNNING = 1,     /* 运行状态 */
    TIMER_EXPIRED = 2      /* 已过期状态 */
} timer_state_t;

/* 定时器回调函数类型 */
typedef void (*timer_callback_t)(void *arg);

/* 定时器句柄 */
typedef struct smart_timer* timer_handle_t;

/* 定时器控制块 */
typedef struct smart_timer {
    uint32_t id;                    /* 定时器ID */
    timer_type_t type;              /* 定时器类型 */
    timer_state_t state;            /* 定时器状态 */
    uint32_t period_ms;             /* 定时周期(毫秒) */
    uint32_t remaining_ms;          /* 剩余时间(毫秒) */
    timer_callback_t callback;      /* 回调函数 */
    void *callback_arg;             /* 回调参数 */
    struct smart_timer *next;       /* 链表指针 */
} smart_timer_t;

/* 定时器统计信息 */
typedef struct {
    uint32_t total_timers;          /* 总定时器数量 */
    uint32_t active_timers;         /* 活跃定时器数量 */
    uint32_t expired_count;         /* 过期次数统计 */
    uint32_t callback_count;        /* 回调执行次数 */
    uint32_t max_callback_time_us;  /* 最大回调执行时间(微秒) */
} timer_stats_t;

/* 初始化定时器系统 */
void smart_timer_init(void);

/* 创建定时器 */
timer_handle_t smart_timer_create(timer_type_t type, uint32_t period_ms, 
                                  timer_callback_t callback, void *arg);

/* 启动定时器 */
int smart_timer_start(timer_handle_t timer);

/* 停止定时器 */
int smart_timer_stop(timer_handle_t timer);

/* 重置定时器 */
int smart_timer_reset(timer_handle_t timer);

/* 删除定时器 */
int smart_timer_delete(timer_handle_t timer);

/* 修改定时器周期 */
int smart_timer_set_period(timer_handle_t timer, uint32_t period_ms);

/* 获取定时器剩余时间 */
uint32_t smart_timer_get_remaining(timer_handle_t timer);

/* 获取定时器状态 */
timer_state_t smart_timer_get_state(timer_handle_t timer);

/* 获取统计信息 */
void smart_timer_get_stats(timer_stats_t *stats);

/* 定时器系统滴答处理(由系统定时器调用) */
void smart_timer_tick(void);

/* 列出所有定时器 */
void smart_timer_list(void);

#endif