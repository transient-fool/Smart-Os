#ifndef __SMART_CORE_H__
#define __SMART_CORE_H__

#include <stdint.h>

/* Smart-OS: An EDF (Earliest Deadline First) Scheduler Kernel */

#define TASK_STATE_INIT     0
#define TASK_STATE_READY    1
#define TASK_STATE_RUNNING  2
#define TASK_STATE_WAITING  3  /* 等待下一个周期 */
#define TASK_STATE_DELAYED  5  /* 延时等待 */
#define TASK_STATE_SUSPEND  4  /* 挂起状态 */

typedef uint32_t smart_time_t;

/* 任务控制块 */
struct smart_task {
    void *sp;               /* 栈指针 (必须在首位) */
    
    /* 任务属性 */
    void (*entry)(void*);
    void *parameter;
    void *stack_addr;
    uint32_t stack_size;
    void *stack_guard;
    
    /* EDF 调度核心参数 */
    smart_time_t deadline;  /* 绝对截止时间 */
    smart_time_t period;    /* 周期 (0表示单次任务) */
    smart_time_t arrival;   /* 下一次到达时间 */
    smart_time_t wakeup_time; /* 延时唤醒时间 */
    
    uint8_t state;
    uint32_t switch_count;   /* 被切换出去的次数，用于统计 */
    uint32_t min_free_stack; /* 运行期间观察到的最小可用栈空间（字节） */
    
    /* 执行时间统计与预测 */
    smart_time_t exec_start_time;    /* 任务开始执行时间 */
    smart_time_t last_exec_time;     /* 上次执行时间（实际测量值） */
    smart_time_t avg_exec_time;      /* 平均执行时间（EMA预测值） */
    smart_time_t max_exec_time;      /* 最大执行时间 */
    uint32_t deadline_miss_count;    /* 错过截止时间次数 */
    
    struct smart_task *next;
};

typedef struct smart_task *smart_task_t;

/* 内核 API */
void smart_os_init(void);
void smart_os_start(void);

/* 创建一个周期性任务
 * period: 周期 (ticks)，如果为0则为普通一次性任务(deadline设为最大)
 * relative_deadline: 相对截止时间，通常等于 period
 */
void smart_task_create(smart_task_t task,
                       void (*entry)(void*),
                       void *param,
                       void *stack,
                       uint32_t stack_size,
                       smart_time_t period,
                       smart_time_t relative_deadline);

/* 任务主动放弃 CPU，等待下一个周期 */
void smart_task_yield(void);

/* 临界区保护 */
void smart_enter_critical(void);
void smart_exit_critical(void);

/* 获取当前系统时间 */
smart_time_t smart_get_tick(void);

/* 延时指定 tick 数 */
void smart_delay(smart_time_t ticks);

/* 任务信息查询 */
typedef struct {
    void (*entry)(void*);
    uint32_t deadline;
    uint32_t period;
    uint8_t state;
    uint32_t switch_count;
    uint32_t stack_size;
    uint32_t min_free_stack;
    uint32_t last_exec_time;      /* 上次执行时间 */
    uint32_t avg_exec_time;       /* 平均执行时间（预测值） */
    uint32_t max_exec_time;       /* 最大执行时间 */
    uint32_t deadline_miss_count; /* 错过截止时间次数 */
} smart_task_info_t;

/* 获取任务列表（返回任务数量） */
int smart_get_task_list(smart_task_info_t *info_array, int max_tasks);

/* 获取当前运行任务 */
smart_task_t smart_get_current_task(void);


#endif
