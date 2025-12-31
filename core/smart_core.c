#include "smart_core.h"
#include "smart_uart.h"
#include "smart_mempool.h"
#include "smart_timer.h"

#ifndef SMART_LOG_ENABLED
#define SMART_LOG_ENABLED 0  /* 关闭日志避免刷屏 */
#endif

#ifndef SMART_LOG_LEVEL
#define SMART_LOG_LEVEL 0 /* 0:OFF,1:ERROR,2:INFO,3:DEBUG */
#endif

#if SMART_LOG_ENABLED
#define SMART_LOG_IF(level, msg) do { if ((level) <= SMART_LOG_LEVEL) smart_uart_print(msg); } while (0)
#define SMART_LOG(msg)        SMART_LOG_IF(2, msg)
#define SMART_LOG_DEBUG(msg)  SMART_LOG_IF(3, msg)
static void smart_log_task_info(const char *prefix, smart_task_t task)
{
    SMART_LOG(prefix);
    SMART_LOG(" entry=0x");
    smart_uart_print_hex32((uint32_t)task->entry);
    SMART_LOG(" deadline=0x");
    smart_uart_print_hex32(task->deadline);
    SMART_LOG(" period=0x");
    smart_uart_print_hex32(task->period);
    SMART_LOG("\n");
}
#else
#define SMART_LOG(msg) do {} while (0)
#define SMART_LOG_DEBUG(msg) do {} while (0)
static inline void smart_log_task_info(const char *prefix, smart_task_t task)
{
    (void)prefix;
    (void)task;
}
#endif

#define SYSTICK_CTRL   (*(volatile uint32_t *)0xE000E010)
#define SYSTICK_LOAD   (*(volatile uint32_t *)0xE000E014)
#define SYSTICK_VAL    (*(volatile uint32_t *)0xE000E018)
#define SCB_SHPR3      (*(volatile uint32_t *)0xE000ED20)

#define STACK_GUARD_PATTERN 0xDEADBEEF

/* 全局变量 */
smart_task_t current_task = 0;
smart_task_t next_task = 0;
smart_task_t task_list = 0; /* 任务链表 */
static int scheduler_started = 0;

volatile smart_time_t os_tick = 0;

/* 临界区嵌套计数 */
static volatile uint32_t critical_nesting = 0;

/* Idle任务 */
static struct smart_task idle_task;
static uint8_t idle_stack[256];
static void idle_task_entry(void *param);

/* 栈保护 */
static void smart_stack_guard_init(smart_task_t task)
{
    if (!task || !task->stack_addr) return;
    
    uintptr_t guard_addr = (uintptr_t)task->stack_addr;
    guard_addr = (guard_addr + 3u) & ~0x3u; /* 4字节对齐 */
    task->stack_guard = (void *)guard_addr;
    *(volatile uint32_t *)task->stack_guard = STACK_GUARD_PATTERN;
}

static void smart_stack_guard_check(smart_task_t task)
{
    if (!task || !task->stack_guard) return;
    
    if (*(volatile uint32_t *)task->stack_guard != STACK_GUARD_PATTERN)
    {
        smart_uart_print("[SmartOS][Fatal] Stack overflow detected!\n");
        smart_uart_print("  Task entry: 0x");
        smart_uart_print_hex32((uint32_t)task->entry);
        smart_uart_print("\n");
        
        smart_uart_print("  Stack addr: 0x");
        smart_uart_print_hex32((uint32_t)task->stack_addr);
        smart_uart_print("\n");
        
        smart_uart_print("  Guard addr: 0x");
        smart_uart_print_hex32((uint32_t)task->stack_guard);
        smart_uart_print("\n");
        
        smart_uart_print("System halted.\n");
        while(1);
    }
}

/* 栈水位记录：计算当前栈剩余空间并更新最小值 */
static void smart_task_update_watermark(smart_task_t task)
{
    if (!task || !task->stack_addr || !task->sp) return;
    uintptr_t sp = (uintptr_t)task->sp;
    uintptr_t base = (uintptr_t)task->stack_addr;
    if (sp > base)
    {
        uint32_t free_bytes = (uint32_t)(sp - base);
        if (free_bytes < task->min_free_stack)
        {
            task->min_free_stack = free_bytes;
        }
    }
}

#if SMART_LOG_ENABLED
static void smart_log_task_watermarks(void)
{
    smart_task_t node = task_list;
    while(node)
    {
        SMART_LOG_DEBUG("[SmartOS][Stack] entry=0x");
        smart_uart_print_hex32((uint32_t)node->entry);
        SMART_LOG_DEBUG(" min_free=");
        smart_uart_print_hex32(node->min_free_stack);
        SMART_LOG_DEBUG("\n");
        node = node->next;
    }
}
#endif

/* 外部汇编函数 */
extern void trigger_pend_sv(void);
extern void start_first_task(void);

/* 内联汇编：中断控制 */
static inline void __smart_disable_irq(void)
{
    __asm volatile ("CPSID I" ::: "memory");
}

static inline void __smart_enable_irq(void)
{
    __asm volatile ("CPSIE I" ::: "memory");
}

/* 临界区保护 */
void smart_enter_critical(void)
{
    __smart_disable_irq();
    critical_nesting++;
}

void smart_exit_critical(void)
{
    if (critical_nesting > 0)
    {
        critical_nesting--;
        if (critical_nesting == 0)
        {
            __smart_enable_irq();
        }
    }
}

/* Cortex-M3 栈初始化 */
static uint8_t *hw_stack_init(void (*tentry)(void*), void *parameter, uint8_t *stack_addr)
{
    unsigned long *stk = (unsigned long *)stack_addr;
    
    *(--stk) = 0x01000000L; /* xPSR */
    
    /* 关键修正：强制设置 PC 的 Bit 0 为 1，确保处于 Thumb 模式 */
    *(--stk) = (unsigned long)tentry | 1; /* PC */
    
    *(--stk) = 0; /* LR */
    *(--stk) = 0; /* R12 */
    *(--stk) = 0; /* R3 */
    *(--stk) = 0; /* R2 */
    *(--stk) = 0; /* R1 */
    *(--stk) = (unsigned long)parameter; /* R0 */
    
    /* R4-R11 */
    for(int i=0; i<8; i++) *(--stk) = 0;
    
    return (uint8_t *)stk;
}

void smart_os_init(void)
{
    current_task = 0;
    task_list = 0;
    next_task = 0;
    os_tick = 0;
    scheduler_started = 0;
    critical_nesting = 0;

    /* 初始化软件定时器系统 */
    smart_timer_init();

    /* 初始化Idle任务 */
    smart_task_create(&idle_task, idle_task_entry, 0,
                      idle_stack, sizeof(idle_stack),
                      0, 0xFFFFFFFF);
    idle_task.state = TASK_STATE_READY;
}

void smart_task_create(smart_task_t task,
                       void (*entry)(void*),
                       void *param,
                       void *stack,
                       uint32_t stack_size,
                       smart_time_t period,
                       smart_time_t relative_deadline)
{
    smart_uart_init();
    smart_enter_critical();

    /* 初始化栈 */
    uint8_t *ptr = (uint8_t *)stack + stack_size;
    while((uint32_t)ptr & 7) ptr--;
    task->sp = hw_stack_init(entry, param, ptr);
    
    task->entry = entry;
    task->parameter = param;
    task->stack_addr = stack;
    task->stack_size = stack_size;
    task->stack_guard = 0;
    task->period = period;
    task->switch_count = 0;
    task->min_free_stack = stack_size;
    
    /* 初始化执行时间统计 */
    task->exec_start_time = 0;
    task->last_exec_time = 0;
    task->avg_exec_time = 0;
    task->max_exec_time = 0;
    task->deadline_miss_count = 0;
    
    /* 初始化时间属性 */
    task->arrival = os_tick; 
    task->wakeup_time = 0;
    if (period > 0)
        task->deadline = os_tick + relative_deadline;
    else
        task->deadline = 0xFFFFFFFF; /* 非周期任务，截止时间无穷大 */
        
    task->state = TASK_STATE_READY;
    
    /* 栈守卫 */
    smart_stack_guard_init(task);

    /* 插入链表 */
    task->next = task_list;
    task_list = task;

    smart_exit_critical();

    smart_log_task_info("[SmartOS] Task created", task);
}

/* EDF 调度器：寻找 Ready 且 Deadline 最小的任务 */
void smart_schedule(void)
{
    smart_task_t node = task_list;
    smart_task_t best = 0;
    smart_time_t min_deadline = 0xFFFFFFFF;
    
    while(node)
    {
        if (node->state == TASK_STATE_READY)
        {
            if (node->deadline < min_deadline)
            {
                min_deadline = node->deadline;
                best = node;
            }
        }
        node = node->next;
    }
    
    /* 如果没有任务就绪，运行 Idle 任务 */
    if (best == 0)
    {
        if (next_task != &idle_task)
        {
            SMART_LOG("[SmartOS] No READY task, switch to Idle\n");
        }
        best = &idle_task;
    }
    
    next_task = best;

    if (!scheduler_started)
    {
        /* 首次调度，仅记录当前任务，等待 start_first_task 启动 */
        if (current_task == 0)
        {
            current_task = best;
        }
        return;
    }

    if (current_task != next_task)
    {
        if (current_task)
        {
            /* 记录任务执行时间 */
            smart_time_t exec_time = os_tick - current_task->exec_start_time;
            current_task->last_exec_time = exec_time;
            
            /* 更新最大执行时间 */
            if (exec_time > current_task->max_exec_time)
            {
                current_task->max_exec_time = exec_time;
            }
            
            /* EMA算法更新平均执行时间：avg = alpha * new + (1-alpha) * avg
             * 使用定点运算：alpha = 1/8 = 0.125
             * avg = (new + 7*avg) / 8
             */
            if (current_task->avg_exec_time == 0)
            {
                current_task->avg_exec_time = exec_time;
            }
            else
            {
                current_task->avg_exec_time = (exec_time + 7 * current_task->avg_exec_time) / 8;
            }
            
            /* 检测deadline miss */
            if (os_tick > current_task->deadline && current_task->period > 0)
            {
                current_task->deadline_miss_count++;
            }
            
            smart_task_update_watermark(current_task);
            current_task->switch_count++;
        }
        
        /* 记录新任务开始执行时间 */
        if (next_task)
        {
            next_task->exec_start_time = os_tick;
        }
        
        smart_stack_guard_check(current_task);
        smart_stack_guard_check(next_task);
        smart_log_task_info("[SmartOS] PendSV trigger, next task", next_task);
        trigger_pend_sv();
    }
}

/* 任务完成当前周期，挂起并等待下一周期 */
void smart_task_yield(void)
{
    smart_enter_critical();
    
    if (current_task->period > 0)
    {
        current_task->state = TASK_STATE_WAITING;
        current_task->arrival += current_task->period;
        current_task->deadline += current_task->period;
    }
    else
    {
        /* 非周期任务 Yield 后仍然 Ready，只是让出 CPU */
        current_task->state = TASK_STATE_READY; 
    }
    
    smart_schedule();
    smart_exit_critical();

    smart_log_task_info("[SmartOS] Task yield", current_task);
}

smart_time_t smart_get_tick(void)
{
    return os_tick;
}

smart_time_t smart_get_tick_count(void)
{
    return os_tick;
}

/* 延时指定 tick 数 */
void smart_delay(smart_time_t ticks)
{
    if (ticks == 0) return;
    
    smart_enter_critical();
    
    if (current_task)
    {
        current_task->wakeup_time = os_tick + ticks;
        current_task->state = TASK_STATE_DELAYED;
        
        SMART_LOG("[SmartOS] Task delay ");
        smart_uart_print_hex32(ticks);
        SMART_LOG(" ticks, wakeup at ");
        smart_uart_print_hex32(current_task->wakeup_time);
        SMART_LOG("\n");
        
        smart_schedule();
    }
    
    smart_exit_critical();
}

/* SVC Handler 的 C 部分 - 用于调试 */
void SVC_Handler_C(void)
{
    smart_uart_print("SVC called!\n");
    
    /* 读取 PSP */
    unsigned long psp;
    __asm volatile ("MRS %0, PSP" : "=r" (psp));
    smart_uart_print("PSP: 0x");
    smart_uart_print_hex32((uint32_t)psp);
    smart_uart_print("\n");
    
    smart_uart_print("About to return from SVC...\n");
}

/* 引用外部计数器 */
extern volatile int count_a;
extern volatile int count_b;

/* SysTick 中断处理 */
void SysTick_Handler(void)
{
    os_tick++;
    smart_mempool_tick();
    smart_timer_tick();  /* 处理软件定时器 */
    
    /* 每 1000 ticks (1秒) 打印一次计数器状态（仅在测试模式下） */
#ifndef ENABLE_SHELL
    if ((os_tick % 1000) == 0)
    {
        smart_uart_putc('A'); 
        smart_uart_putc(':');
        /* 简易打印数字的最后一位，仅作存活证明 */
        smart_uart_putc('0' + (count_a % 10));
        smart_uart_putc(' ');
        smart_uart_putc('B');
        smart_uart_putc(':');
        smart_uart_putc('0' + (count_b % 10));
        smart_uart_putc('\n');
    }
#endif

#if SMART_LOG_ENABLED
    if ((os_tick % 2000) == 0 && SMART_LOG_LEVEL >= 3)
    {
        smart_log_task_watermarks();
    }
#endif
    
    /* 检查是否有等待的任务到达了 Arrival Time */
    smart_task_t node = task_list;
    int need_sched = 0;
    
    while(node)
    {
        if (node->state == TASK_STATE_WAITING)
        {
            if (os_tick >= node->arrival)
            {
                node->state = TASK_STATE_READY;
                need_sched = 1;
            }
        }
        else if (node->state == TASK_STATE_DELAYED)
        {
            if (os_tick >= node->wakeup_time)
            {
                node->state = TASK_STATE_READY;
                need_sched = 1;
                
                SMART_LOG("[SmartOS] Task delay expired, wakeup\n");
            }
        }
        node = node->next;
    }
    
    if (need_sched)
    {
        smart_schedule();
    }
}


void smart_os_start(void)
{
    /* 初始化串口 */
    smart_uart_init();

    /* 配置 SysTick 及优先级，假设时钟 12MHz, 1ms 中断 */
    SYSTICK_LOAD = 12000 - 1;
    SYSTICK_VAL = 0;
    SYSTICK_CTRL = 0x07;

    /* PendSV 设为最低优先级，SysTick 次低 */
    uint32_t shpr3 = SCB_SHPR3;
    shpr3 &= ~((0xFFu << 16) | (0xFFu << 24));
    shpr3 |= (0xFFu << 16);
    shpr3 |= (0xFEu << 24);
    SCB_SHPR3 = shpr3;
    
    smart_uart_print("Smart-OS Starting...\n");

    smart_schedule();
    if (current_task)
    {
        smart_uart_print("First task found. Jumping...\n");
        
        /* 调试：打印任务地址 */
        smart_uart_print("Task entry: 0x");
        smart_uart_print_hex32((uint32_t)current_task->entry);
        smart_uart_print("\n");
        
        smart_uart_print("Calling start_first_task...\n");
        
        /* 调试：打印任务栈指针 */
        smart_uart_print("Task SP: 0x");
        smart_uart_print_hex32((uint32_t)current_task->sp);
        smart_uart_print("\n");
        
        /* 打印栈内容 */
        smart_uart_print("Stack content at SP:\n");
        unsigned long *stack_ptr = (unsigned long *)current_task->sp;
        for(int i=0; i<16; i++) {
            smart_uart_print("  [");
            smart_uart_putc('0' + i/10);
            smart_uart_putc('0' + i%10);
            smart_uart_print("]: 0x");
            smart_uart_print_hex32((uint32_t)stack_ptr[i]);
            smart_uart_print("\n");
        }
        
        scheduler_started = 1;
        start_first_task();
        smart_uart_print("Returned from start_first_task!\n");
    }
    else
    {
        smart_uart_print("No task to run!\n");
    }
    
    smart_uart_print("Fatal: OS exited!\n");
    while(1);
}

static void idle_task_entry(void *param)
{
    (void)param;

    SMART_LOG("[SmartOS] Idle task running\n");
    while(1)
    {
        __asm volatile ("WFI");
    }
}

/* 获取任务列表 */
int smart_get_task_list(smart_task_info_t *info_array, int max_tasks)
{
    if (!info_array || max_tasks <= 0)
    {
        return 0;
    }
    
    smart_enter_critical();
    
    int count = 0;
    smart_task_t node = task_list;
    
    while (node && count < max_tasks)
    {
        info_array[count].entry = node->entry;
        info_array[count].deadline = node->deadline;
        info_array[count].period = node->period;
        info_array[count].state = node->state;
        info_array[count].switch_count = node->switch_count;
        info_array[count].stack_size = node->stack_size;
        info_array[count].min_free_stack = node->min_free_stack;
        info_array[count].last_exec_time = node->last_exec_time;
        info_array[count].avg_exec_time = node->avg_exec_time;
        info_array[count].max_exec_time = node->max_exec_time;
        info_array[count].deadline_miss_count = node->deadline_miss_count;
        
        count++;
        node = node->next;
    }
    
    smart_exit_critical();
    
    return count;
}

/* 获取当前运行任务 */
smart_task_t smart_get_current_task(void)
{
    return current_task;
}
