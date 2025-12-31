#include <stddef.h>

#include "smart_core.h"
#include "smart_uart.h"
#include "smart_mempool.h"
#include "smart_block.h"
#include "smart_fs.h"
#include "smart_shell.h"
#include "smart_msgqueue.h"
#include "smart_banner.h"

/* 功能开关 */
#define ENABLE_SHELL                1
#define ENABLE_MSGQUEUE_DEMO        0  /* 消息队列演示 */
#define ENABLE_DELAY_TEST           0
#define ENABLE_STACK_OVERFLOW_TEST  0

/* 任务栈 */
uint8_t stack_a[1024];
uint8_t stack_b[1024];
uint8_t stack_shell[2048];  /* Shell需要更大栈空间（文件系统操作） */

struct smart_task task_a;
struct smart_task task_b;
struct smart_task task_shell;

/* 全局计数器，用于观察任务运行 */
volatile int count_a = 0;
volatile int count_b = 0;

#define MEMPOOL_BLOCK_SIZE   64u
#define MEMPOOL_BLOCK_COUNT  8u
#define MEMPOOL_OPS_PER_TICK 2u

static uint8_t telemetry_pool_buf[MEMPOOL_BLOCK_SIZE * MEMPOOL_BLOCK_COUNT];
static smart_mempool_t telemetry_pool;
static void *task_a_block = 0;
static void *task_b_block = 0;

/* 导出内存池指针供Shell访问 */
smart_mempool_t *smart_get_mempool(void)
{
    return &telemetry_pool;
}

/* 文件系统测试：内置 Flash（真实硬件存储） */
static smart_block_device_t *flash_dev = 0;

/* 导出Flash设备指针供Shell访问 */
smart_block_device_t *smart_get_flash_device(void)
{
    return flash_dev;
}

static void log_mempool_result(const char *task_name,
                               const char *action,
                               smart_mempool_status_t status)
{
    smart_uart_print("[MemPool][");
    smart_uart_print(task_name);
    smart_uart_print("] ");
    smart_uart_print(action);
    smart_uart_print(" -> ");
    switch(status)
    {
        case SMART_MEMPOOL_OK:
            smart_uart_print("OK");
            break;
        case SMART_MEMPOOL_EMPTY:
            smart_uart_print("EMPTY");
            break;
        case SMART_MEMPOOL_BUSY:
            smart_uart_print("BUSY");
            break;
        case SMART_MEMPOOL_INVALID:
        default:
            smart_uart_print("INVALID");
            break;
    }
    smart_uart_print("\n");
}

static void simulate_stack_overflow(void)
{
#if ENABLE_STACK_OVERFLOW_TEST
    smart_uart_print("[TaskA] Simulating stack overflow...\n");

    smart_enter_critical();
    if (task_a.stack_guard)
    {
        smart_uart_print("  Guard addr: 0x");
        smart_uart_print_hex32((uint32_t)task_a.stack_guard);
        smart_uart_print("\n");
        smart_uart_print("  Corrupting guard...\n");
        *(volatile uint32_t *)task_a.stack_guard = 0u;
    }
    smart_exit_critical();

    smart_uart_print("[TaskA] Guard corrupted, waiting for detector.\n");
#endif
}

void task_a_entry(void *param)
{
    smart_uart_print("Task A started!\n");
    
    while(1)
    {
        count_a++;

#if !ENABLE_STACK_OVERFLOW_TEST
        if ((count_a % 3) == 0 && task_a_block == 0)
        {
            void *blk = 0;
            smart_mempool_status_t st = smart_mempool_alloc_try(&telemetry_pool, &blk);
            log_mempool_result("TaskA", "alloc", st);
            if (st == SMART_MEMPOOL_OK)
            {
                task_a_block = blk;
                ((uint32_t *)task_a_block)[0] = (uint32_t)count_a;
            }
        }

        if (task_a_block && (count_a % 5) == 0)
        {
            smart_mempool_status_t st = smart_mempool_free_try(&telemetry_pool, task_a_block);
            log_mempool_result("TaskA", "free", st);
            if (st == SMART_MEMPOOL_OK)
            {
                task_a_block = 0;
            }
        }
#endif

#if ENABLE_STACK_OVERFLOW_TEST
        if (count_a == 10)
        {
            simulate_stack_overflow();
        }
        smart_task_yield();
#elif ENABLE_DELAY_TEST
        if (count_a % 5 == 0)
        {
            smart_uart_print("[TaskA] Before delay 100ms, tick=");
            smart_uart_print_hex32(smart_get_tick());
            smart_uart_print("\n");
            
            smart_delay(100);
            
            smart_uart_print("[TaskA] After delay, tick=");
            smart_uart_print_hex32(smart_get_tick());
            smart_uart_print("\n");
        }
        else
        {
            smart_task_yield();
        }
#else
        smart_task_yield();
#endif
    }
}

void task_b_entry(void *param)
{
    while(1)
    {
        count_b++;

#if !ENABLE_STACK_OVERFLOW_TEST
        if (task_b_block == 0)
        {
            void *blk = 0;
            smart_mempool_status_t st = smart_mempool_alloc_try(&telemetry_pool, &blk);
            log_mempool_result("TaskB", "alloc", st);
            if (st == SMART_MEMPOOL_OK)
            {
                task_b_block = blk;
            }
        }
        else if ((count_b % 4) == 0)
        {
            smart_mempool_status_t st = smart_mempool_free_try(&telemetry_pool, task_b_block);
            log_mempool_result("TaskB", "free", st);
            if (st == SMART_MEMPOOL_OK)
            {
                task_b_block = 0;
            }
        }
#endif
        smart_task_yield();
    }
}

int main(void)
{
    smart_os_init();
    smart_uart_init();
    
    /* 显示启动横幅 */
    smart_print_banner();
    smart_print_boot_animation();
    
    smart_mempool_init(&telemetry_pool,
                       telemetry_pool_buf,
                       MEMPOOL_BLOCK_SIZE,
                       MEMPOOL_BLOCK_COUNT,
                       MEMPOOL_OPS_PER_TICK);
    
    /* 文件系统测试：初始化内置 Flash（真实硬件存储） */
    smart_uart_print("\n=== File System Test ===\n");
    flash_dev = smart_flash_init();
    if (flash_dev)
    {
        smart_uart_print("[FS] Flash initialized: ");
        smart_uart_print_hex32(flash_dev->total_sectors);
        smart_uart_print(" sectors\n");
        
        /* 尝试初始化文件系统（如果已格式化） */
        smart_uart_print("[FS] Initializing file system...\n");
        smart_fs_status_t fs_status = smart_fs_init(flash_dev);
        
        if (fs_status == SMART_FS_OK)
        {
            smart_uart_print("[FS] File system initialized\n");
            smart_fs_list_dir("/");
        }
        else if (fs_status == SMART_FS_NOT_FOUND)
        {
            /* Flash 未格式化，进行格式化 */
            smart_uart_print("[FS] Flash not formatted, formatting...\n");
            if (smart_fs_format(flash_dev) == SMART_FS_OK)
            {
                smart_uart_print("[FS] Flash formatted successfully\n");
                if (smart_fs_init(flash_dev) == SMART_FS_OK)
                {
                    smart_uart_print("[FS] File system initialized\n");
                    smart_fs_list_dir("/");
                }
                else
                {
                    smart_uart_print("[FS] File system init failed after format\n");
                }
            }
            else
            {
                smart_uart_print("[FS] Flash format failed\n");
            }
        }
        else
        {
            smart_uart_print("[FS] File system init error\n");
        }
    }
    else
    {
        smart_uart_print("[FS] Flash init failed\n");
    }
    smart_uart_print("=== End FS Test ===\n\n");
    
    /* 调试：打印栈地址 */
    smart_uart_print("stack_a address: 0x");
    smart_uart_print_hex32((uint32_t)stack_a);
    smart_uart_print("\n");
    smart_uart_print("stack_b address: 0x");
    smart_uart_print_hex32((uint32_t)stack_b);
    smart_uart_print("\n");
    
#if ENABLE_SHELL
    /* Shell 任务：低优先级，不影响实时任务 */
    smart_uart_print("\n[Main] Creating Shell task...\n");
    smart_task_create(&task_shell, shell_task_entry, 0,
                      stack_shell, sizeof(stack_shell),
                      100, 0xFFFFFFFE);  /* 周期100ms检查输入，优先级略高于 Idle */
#else
    /* Task A: 周期 500ms */
    smart_task_create(&task_a, task_a_entry, 0, 
                      stack_a, sizeof(stack_a), 
                      500, 500);
                      
    /* Task B: 周期 1000ms */
    smart_task_create(&task_b, task_b_entry, 0, 
                      stack_b, sizeof(stack_b), 
                      1000, 1000);
#endif
    
    smart_os_start();
    
    return 0;
}
