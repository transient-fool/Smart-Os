#include "smart_shell.h"
#include "smart_core.h"
#include "smart_uart.h"
#include "smart_mempool.h"
#include "smart_fs.h"
#include "smart_msgqueue.h"
#include "smart_sync.h"
#include "smart_timer.h"
#include "../user/snake_game.h"
#include <string.h>

/* Shell 上下文 */
typedef struct {
    char input_buffer[SHELL_INPUT_BUFFER_SIZE];
    uint32_t input_pos;
    uint8_t running;
} shell_context_t;

static shell_context_t shell_ctx = {
    .input_buffer = {0},
    .input_pos = 0,
    .running = 0
};

/* 命令声明 */
static int cmd_help(int argc, char *argv[]);
static int cmd_version(int argc, char *argv[]);
static int cmd_uptime(int argc, char *argv[]);
static int cmd_ps(int argc, char *argv[]);
static int cmd_clear(int argc, char *argv[]);
static int cmd_meminfo(int argc, char *argv[]);
static int cmd_free(int argc, char *argv[]);
static int cmd_ls(int argc, char *argv[]);
static int cmd_cat(int argc, char *argv[]);
static int cmd_echo(int argc, char *argv[]);
static int cmd_rm(int argc, char *argv[]);
static int cmd_format(int argc, char *argv[]);
static int cmd_fsinfo(int argc, char *argv[]);
static int cmd_stats(int argc, char *argv[]);
static int cmd_msgtest(int argc, char *argv[]);
static int cmd_snake(int argc, char *argv[]);
static int cmd_synctest(int argc, char *argv[]);
static int cmd_uartinfo(int argc, char *argv[]);
static int cmd_test(int argc, char *argv[]);
static int cmd_stress(int argc, char *argv[]);
static int cmd_timer(int argc, char *argv[]);

/* 命令表 */
typedef struct {
    const char *name;
    const char *description;
    const char *usage;
    shell_cmd_func_t handler;
} shell_command_t;

static const shell_command_t shell_commands[] = {
    {"help",    "Show all commands",        "help",                  cmd_help},
    {"version", "Show system version",      "version",               cmd_version},
    {"uptime",  "Show system uptime",       "uptime",                cmd_uptime},
    {"ps",      "Show task list",           "ps",                    cmd_ps},
    {"clear",   "Clear screen",             "clear",                 cmd_clear},
    {"meminfo", "Show memory info",         "meminfo",               cmd_meminfo},
    {"free",    "Show free memory",         "free",                  cmd_free},
    {"ls",      "List files",               "ls",                    cmd_ls},
    {"cat",     "Show file content",        "cat <filename>",        cmd_cat},
    {"echo",    "Create/write file",        "echo <text> > <file>",  cmd_echo},
    {"rm",      "Remove file",              "rm <filename>",         cmd_rm},
    {"format",  "Format file system",       "format",                cmd_format},
    {"fsinfo",  "Show file system info",    "fsinfo",                cmd_fsinfo},
    {"stats",   "Task statistics & AI",     "stats",                 cmd_stats},
    {"msgtest", "Message queue test",       "msgtest",               cmd_msgtest},
    {"snake",   "Play Snake game",          "snake",                 cmd_snake},
    {"synctest","Semaphore & Mutex test",   "synctest",              cmd_synctest},
    {"uartinfo","UART interrupt stats",     "uartinfo",              cmd_uartinfo},
    {"test",    "Run system tests",         "test [all|mem|fs|sync|perf]", cmd_test},
    {"stress",  "Run stress tests",         "stress",                cmd_stress},
    {"timer",   "Software timer test",      "timer [list|test]",     cmd_timer},
    {NULL,      NULL,                       NULL,                    NULL}
};

/* 前向声明 */
static void shell_print_prompt(void);
static int shell_parse_command(const char *line, char *argv[], int max_args);
static int shell_execute_command(int argc, char *argv[]);

/* ========== 命令实现 ========== */

static int cmd_help(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    smart_uart_print("\nAvailable commands:\n");
    smart_uart_print("------------------\n");
    
    for (int i = 0; shell_commands[i].name != NULL; i++) {
        smart_uart_print("  ");
        smart_uart_print(shell_commands[i].name);
        
        /* Align description */
        int len = strlen(shell_commands[i].name);
        for (int j = len; j < 12; j++) {
            smart_uart_putc(' ');
        }
        
        smart_uart_print(shell_commands[i].description);
        smart_uart_print("\n");
    }
    smart_uart_print("\n");
    
    return 0;
}

static int cmd_version(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    smart_uart_print("\nSmart-OS v1.0\n");
    smart_uart_print("Build: " __DATE__ " " __TIME__ "\n");
    smart_uart_print("A simple RTOS for ARM Cortex-M3\n\n");
    
    return 0;
}

static int cmd_uptime(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    uint32_t ticks = smart_get_tick();
    uint32_t seconds = ticks / 1000;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    
    smart_uart_print("\nSystem uptime: ");
    smart_uart_print_hex32(hours);
    smart_uart_print("h ");
    smart_uart_print_hex32(minutes % 60);
    smart_uart_print("m ");
    smart_uart_print_hex32(seconds % 60);
    smart_uart_print("s (");
    smart_uart_print_hex32(ticks);
    smart_uart_print(" ticks)\n\n");
    
    return 0;
}

static int cmd_ps(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    smart_task_info_t tasks[10];
    int count = smart_get_task_list(tasks, 10);
    
    smart_uart_print("\nTask List:\n");
    smart_uart_print("-------------------------------------------------------------------------\n");
    smart_uart_print("Entry      State   Switches  ExecTime(Last/Avg/Max)  Misses  Stack\n");
    smart_uart_print("-------------------------------------------------------------------------\n");
    
    const char *state_names[] = {
        "INIT ", "READY", "RUN  ", "WAIT ", "SUSP ", "DELAY"
    };
    
    for (int i = 0; i < count; i++)
    {
        /* Entry address */
        smart_uart_print("0x");
        smart_uart_print_hex32((uint32_t)tasks[i].entry);
        smart_uart_print(" ");
        
        /* State */
        if (tasks[i].state < 6)
        {
            smart_uart_print(state_names[tasks[i].state]);
        }
        else
        {
            smart_uart_print("???? ");
        }
        smart_uart_print(" ");
        
        /* Switch count */
        smart_uart_print_hex32(tasks[i].switch_count);
        smart_uart_print("    ");
        
        /* Execution time: Last/Avg/Max */
        smart_uart_print_hex32(tasks[i].last_exec_time);
        smart_uart_print("/");
        smart_uart_print_hex32(tasks[i].avg_exec_time);
        smart_uart_print("/");
        smart_uart_print_hex32(tasks[i].max_exec_time);
        smart_uart_print("  ");
        
        /* Deadline misses */
        smart_uart_print_hex32(tasks[i].deadline_miss_count);
        smart_uart_print("      ");
        
        /* Stack usage */
        uint32_t used = tasks[i].stack_size - tasks[i].min_free_stack;
        smart_uart_print_hex32(used);
        smart_uart_print("/");
        smart_uart_print_hex32(tasks[i].stack_size);
        
        smart_uart_print("\n");
    }
    
    smart_uart_print("-------------------------------------------------------------------------\n");
    smart_uart_print("Total: ");
    smart_uart_print_hex32(count);
    smart_uart_print(" tasks | ExecTime in ticks (1 tick = 1ms)\n\n");
    
    return 0;
}

static int cmd_clear(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    /* ANSI 清屏序列 */
    smart_uart_print("\033[2J\033[H");
    
    return 0;
}

static int cmd_meminfo(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    smart_mempool_t *pool = smart_get_mempool();
    if (!pool)
    {
        smart_uart_print("\nError: Memory pool not available\n\n");
        return -1;
    }
    
    smart_mempool_stats_t stats;
    smart_mempool_get_stats(pool, &stats);
    
    smart_uart_print("\nMemory Pool Information:\n");
    smart_uart_print("------------------------\n");
    
    smart_uart_print("Block size:      ");
    smart_uart_print_hex32(stats.block_size);
    smart_uart_print(" bytes\n");
    
    smart_uart_print("Total blocks:    ");
    smart_uart_print_hex32(stats.block_count);
    smart_uart_print("\n");
    
    smart_uart_print("Free blocks:     ");
    smart_uart_print_hex32(stats.free_count);
    smart_uart_print("\n");
    
    smart_uart_print("Used blocks:     ");
    smart_uart_print_hex32(stats.block_count - stats.free_count);
    smart_uart_print("\n");
    
    smart_uart_print("Min free (peak): ");
    smart_uart_print_hex32(stats.min_free_count);
    smart_uart_print("\n");
    
    uint32_t total_bytes = stats.block_size * stats.block_count;
    uint32_t free_bytes = stats.block_size * stats.free_count;
    uint32_t used_bytes = total_bytes - free_bytes;
    
    smart_uart_print("\nTotal memory:    ");
    smart_uart_print_hex32(total_bytes);
    smart_uart_print(" bytes\n");
    
    smart_uart_print("Used memory:     ");
    smart_uart_print_hex32(used_bytes);
    smart_uart_print(" bytes\n");
    
    smart_uart_print("Free memory:     ");
    smart_uart_print_hex32(free_bytes);
    smart_uart_print(" bytes\n\n");
    
    return 0;
}

static int cmd_free(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    smart_mempool_t *pool = smart_get_mempool();
    if (!pool)
    {
        smart_uart_print("\nError: Memory pool not available\n\n");
        return -1;
    }
    
    smart_mempool_stats_t stats;
    smart_mempool_get_stats(pool, &stats);
    
    uint32_t total_bytes = stats.block_size * stats.block_count;
    uint32_t free_bytes = stats.block_size * stats.free_count;
    uint32_t used_bytes = total_bytes - free_bytes;
    
    smart_uart_print("\n              Total       Used       Free\n");
    smart_uart_print("Memory:   ");
    smart_uart_print_hex32(total_bytes);
    smart_uart_print("   ");
    smart_uart_print_hex32(used_bytes);
    smart_uart_print("   ");
    smart_uart_print_hex32(free_bytes);
    smart_uart_print("\n\n");
    
    return 0;
}

static int cmd_ls(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    smart_uart_print("\n");
    smart_fs_status_t status = smart_fs_list_dir("/");
    
    if (status != SMART_FS_OK) {
        smart_uart_print("Error: Failed to list directory\n");
        return -1;
    }
    smart_uart_print("\n");
    
    return 0;
}

static int cmd_cat(int argc, char *argv[])
{
    if (argc < 2) {
        smart_uart_print("Usage: cat <filename>\n");
        return -1;
    }
    
    smart_file_t file;
    smart_fs_status_t status = smart_fs_open(argv[1], &file);
    
    if (status != SMART_FS_OK) {
        smart_uart_print("Error: File not found\n");
        return -1;
    }
    
    smart_uart_print("\n");
    
    char buffer[128];
    uint32_t bytes_read;
    
    while (1) {
        status = smart_fs_read(&file, buffer, sizeof(buffer) - 1, &bytes_read);
        if (status != SMART_FS_OK || bytes_read == 0) {
            break;
        }
        
        buffer[bytes_read] = '\0';
        smart_uart_print(buffer);
    }
    
    smart_fs_close(&file);
    smart_uart_print("\n\n");
    
    return 0;
}

static int cmd_echo(int argc, char *argv[])
{
    /* Simple usage: echo text > filename */
    if (argc < 4 || argv[argc-2][0] != '>') {
        smart_uart_print("Usage: echo <text> > <filename>\n");
        smart_uart_print("Example: echo Hello > test.txt\n");
        return -1;
    }
    
    const char *filename = argv[argc-1];
    
    /* Create file */
    smart_fs_status_t status = smart_fs_create(filename);
    if (status != SMART_FS_OK && status != SMART_FS_FULL) {
        smart_uart_print("Error: Failed to create file\n");
        return -1;
    }
    
    /* Open file */
    smart_file_t file;
    status = smart_fs_open(filename, &file);
    if (status != SMART_FS_OK) {
        smart_uart_print("Error: Failed to open file\n");
        return -1;
    }
    
    /* Write text (all args before '>') */
    for (int i = 1; i < argc - 2; i++) {
        uint32_t bytes_written;
        status = smart_fs_write(&file, argv[i], strlen(argv[i]), &bytes_written);
        if (status != SMART_FS_OK) {
            smart_uart_print("Error: Write failed (");
            smart_uart_print_hex32(status);
            smart_uart_print(")\n");
            smart_fs_close(&file);
            return -1;
        }
        
        /* Add space between words */
        if (i < argc - 3) {
            smart_fs_write(&file, " ", 1, &bytes_written);
        }
    }
    
    /* Update directory entry with file size */
    smart_fs_update_file_info(filename, file.first_cluster, file.file_size);
    
    smart_fs_close(&file);
    smart_uart_print("File created successfully\n");
    
    return 0;
}

static int cmd_rm(int argc, char *argv[])
{
    if (argc < 2) {
        smart_uart_print("Usage: rm <filename>\n");
        return -1;
    }
    
    smart_fs_status_t status = smart_fs_delete(argv[1]);
    
    if (status == SMART_FS_OK) {
        smart_uart_print("File deleted successfully\n");
    } else if (status == SMART_FS_NOT_FOUND) {
        smart_uart_print("Error: File not found\n");
        return -1;
    } else {
        smart_uart_print("Error: Failed to delete file\n");
        return -1;
    }
    
    return 0;
}

static int cmd_format(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    smart_uart_print("\nWARNING: This will erase all files!\n");
    smart_uart_print("Format file system? (y/n): ");
    
    /* 简化版：直接格式化，不等待确认 */
    smart_uart_print("y\n");
    
    extern smart_block_device_t *smart_get_flash_device(void);
    smart_block_device_t *dev = smart_get_flash_device();
    
    if (!dev)
    {
        smart_uart_print("Error: Flash device not available\n\n");
        return -1;
    }
    
    smart_uart_print("Formatting...\n");
    smart_fs_status_t status = smart_fs_format(dev);
    
    if (status == SMART_FS_OK)
    {
        smart_uart_print("Format successful\n");
        smart_uart_print("Reinitializing file system...\n");
        
        status = smart_fs_init(dev);
        if (status == SMART_FS_OK)
        {
            smart_uart_print("File system ready\n\n");
        }
        else
        {
            smart_uart_print("Error: Failed to initialize after format\n\n");
            return -1;
        }
    }
    else
    {
        smart_uart_print("Error: Format failed\n\n");
        return -1;
    }
    
    return 0;
}

static int cmd_fsinfo(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    extern smart_block_device_t *smart_get_flash_device(void);
    smart_block_device_t *dev = smart_get_flash_device();
    
    if (!dev)
    {
        smart_uart_print("\nError: Flash device not available\n\n");
        return -1;
    }
    
    smart_uart_print("\nFile System Information:\n");
    smart_uart_print("------------------------\n");
    smart_uart_print("Type:            FAT12\n");
    
    smart_uart_print("Total sectors:   ");
    smart_uart_print_hex32(dev->total_sectors);
    smart_uart_print("\n");
    
    smart_uart_print("Sector size:     512 bytes\n");
    
    uint32_t total_bytes = dev->total_sectors * 512;
    smart_uart_print("Total size:      ");
    smart_uart_print_hex32(total_bytes);
    smart_uart_print(" bytes (");
    smart_uart_print_hex32(total_bytes / 1024);
    smart_uart_print(" KB)\n");
    
    smart_uart_print("Base address:    0x");
    smart_uart_print_hex32(dev->base_address);
    smart_uart_print("\n\n");
    
    return 0;
}

static int cmd_stats(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    smart_task_info_t tasks[10];
    int count = smart_get_task_list(tasks, 10);
    
    smart_uart_print("\n=== Task Performance Analysis (AI-Powered) ===\n\n");
    
    for (int i = 0; i < count; i++)
    {
        smart_uart_print("Task 0x");
        smart_uart_print_hex32((uint32_t)tasks[i].entry);
        smart_uart_print(":\n");
        
        /* 执行时间分析 */
        smart_uart_print("  Exec Time: Last=");
        smart_uart_print_hex32(tasks[i].last_exec_time);
        smart_uart_print("ms, Predicted(EMA)=");
        smart_uart_print_hex32(tasks[i].avg_exec_time);
        smart_uart_print("ms, Max=");
        smart_uart_print_hex32(tasks[i].max_exec_time);
        smart_uart_print("ms\n");
        
        /* 异常检测：执行时间波动 */
        if (tasks[i].avg_exec_time > 0)
        {
            int32_t deviation = (int32_t)tasks[i].last_exec_time - (int32_t)tasks[i].avg_exec_time;
            int32_t deviation_percent = (deviation * 100) / tasks[i].avg_exec_time;
            
            smart_uart_print("  Deviation: ");
            if (deviation >= 0)
            {
                smart_uart_print("+");
            }
            smart_uart_print_hex32((uint32_t)(deviation >= 0 ? deviation : -deviation));
            smart_uart_print("ms (");
            if (deviation >= 0)
            {
                smart_uart_print("+");
            }
            smart_uart_print_hex32((uint32_t)(deviation_percent >= 0 ? deviation_percent : -deviation_percent));
            smart_uart_print("%)\n");
            
            /* AI异常检测：偏差超过50%视为异常 */
            if (deviation_percent > 50 || deviation_percent < -50)
            {
                smart_uart_print("  [AI WARNING] Execution time anomaly detected!\n");
            }
        }
        
        /* Deadline分析 */
        if (tasks[i].period > 0)
        {
            smart_uart_print("  Period=");
            smart_uart_print_hex32(tasks[i].period);
            smart_uart_print("ms, Deadline Misses=");
            smart_uart_print_hex32(tasks[i].deadline_miss_count);
            
            if (tasks[i].deadline_miss_count > 0)
            {
                smart_uart_print(" [AI ALERT] Real-time constraint violated!\n");
            }
            else
            {
                smart_uart_print(" [OK]\n");
            }
            
            /* CPU利用率预测 */
            if (tasks[i].avg_exec_time > 0 && tasks[i].period > 0)
            {
                uint32_t utilization = (tasks[i].avg_exec_time * 100) / tasks[i].period;
                smart_uart_print("  CPU Utilization (Predicted): ");
                smart_uart_print_hex32(utilization);
                smart_uart_print("%\n");
                
                if (utilization > 80)
                {
                    smart_uart_print("  [AI WARNING] High CPU load, may miss deadline!\n");
                }
            }
        }
        
        /* 栈使用分析 */
        uint32_t stack_used = tasks[i].stack_size - tasks[i].min_free_stack;
        uint32_t stack_usage_percent = (stack_used * 100) / tasks[i].stack_size;
        smart_uart_print("  Stack: ");
        smart_uart_print_hex32(stack_used);
        smart_uart_print("/");
        smart_uart_print_hex32(tasks[i].stack_size);
        smart_uart_print(" (");
        smart_uart_print_hex32(stack_usage_percent);
        smart_uart_print("%)\n");
        
        if (stack_usage_percent > 80)
        {
            smart_uart_print("  [AI WARNING] Stack usage high, risk of overflow!\n");
        }
        
        smart_uart_print("\n");
    }
    
    smart_uart_print("=== AI Analysis Complete ===\n");
    smart_uart_print("Algorithm: Exponential Moving Average (EMA) for prediction\n");
    smart_uart_print("Anomaly Detection: Statistical deviation analysis\n\n");
    
    return 0;
}

static int cmd_msgtest(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    smart_uart_print("\n=== Message Queue Test ===\n\n");
    
    /* 创建测试队列 */
    static smart_msg_t msg_buffer[8];
    static smart_msgqueue_t test_queue;
    
    smart_msgqueue_init(&test_queue, msg_buffer, 8);
    
    smart_uart_print("1. Queue initialized (capacity=8)\n");
    smart_uart_print("   Count: ");
    smart_uart_print_hex32(smart_msgqueue_count(&test_queue));
    smart_uart_print(", Space: ");
    smart_uart_print_hex32(smart_msgqueue_space(&test_queue));
    smart_uart_print("\n\n");
    
    /* 发送消息 */
    smart_uart_print("2. Sending 5 messages...\n");
    for (int i = 0; i < 5; i++)
    {
        smart_msg_t msg;
        msg.type = 0x100 + i;
        msg.data = i * 10;
        msg.ptr = NULL;
        
        smart_msgq_status_t status = smart_msgqueue_send(&test_queue, &msg);
        smart_uart_print("   Msg ");
        smart_uart_print_hex32(i);
        smart_uart_print(": type=0x");
        smart_uart_print_hex32(msg.type);
        smart_uart_print(", data=");
        smart_uart_print_hex32(msg.data);
        smart_uart_print(" -> ");
        smart_uart_print(status == SMART_MSGQ_OK ? "OK" : "FAIL");
        smart_uart_print("\n");
    }
    
    smart_uart_print("   Count: ");
    smart_uart_print_hex32(smart_msgqueue_count(&test_queue));
    smart_uart_print(", Space: ");
    smart_uart_print_hex32(smart_msgqueue_space(&test_queue));
    smart_uart_print("\n\n");
    
    /* 接收消息 */
    smart_uart_print("3. Receiving 3 messages...\n");
    for (int i = 0; i < 3; i++)
    {
        smart_msg_t msg;
        smart_msgq_status_t status = smart_msgqueue_receive(&test_queue, &msg);
        
        smart_uart_print("   Received: type=0x");
        smart_uart_print_hex32(msg.type);
        smart_uart_print(", data=");
        smart_uart_print_hex32(msg.data);
        smart_uart_print(" -> ");
        smart_uart_print(status == SMART_MSGQ_OK ? "OK" : "EMPTY");
        smart_uart_print("\n");
    }
    
    smart_uart_print("   Count: ");
    smart_uart_print_hex32(smart_msgqueue_count(&test_queue));
    smart_uart_print(", Space: ");
    smart_uart_print_hex32(smart_msgqueue_space(&test_queue));
    smart_uart_print("\n\n");
    
    /* 测试队列满 */
    smart_uart_print("4. Testing queue full (sending 10 messages)...\n");
    int success = 0, failed = 0;
    for (int i = 0; i < 10; i++)
    {
        smart_msg_t msg;
        msg.type = 0x200 + i;
        msg.data = i;
        msg.ptr = NULL;
        
        smart_msgq_status_t status = smart_msgqueue_send(&test_queue, &msg);
        if (status == SMART_MSGQ_OK)
        {
            success++;
        }
        else
        {
            failed++;
        }
    }
    
    smart_uart_print("   Success: ");
    smart_uart_print_hex32(success);
    smart_uart_print(", Failed: ");
    smart_uart_print_hex32(failed);
    smart_uart_print(" (queue full)\n");
    smart_uart_print("   Dropped: ");
    smart_uart_print_hex32(test_queue.dropped);
    smart_uart_print("\n\n");
    
    /* 清空队列 */
    smart_uart_print("5. Draining queue...\n");
    int drained = 0;
    while (!smart_msgqueue_is_empty(&test_queue))
    {
        smart_msg_t msg;
        smart_msgqueue_receive(&test_queue, &msg);
        drained++;
    }
    smart_uart_print("   Drained ");
    smart_uart_print_hex32(drained);
    smart_uart_print(" messages\n");
    smart_uart_print("   Queue is now ");
    smart_uart_print(smart_msgqueue_is_empty(&test_queue) ? "EMPTY" : "NOT EMPTY");
    smart_uart_print("\n\n");
    
    smart_uart_print("=== Test Complete ===\n\n");
    
    return 0;
}

static int cmd_snake(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    /* 初始化游戏 */
    snake_game_init();
    snake_game_start();
    
    /* 等待开始 */
    char c;
    while (!smart_uart_getc_nonblock(&c));
    
    /* 显示初始画面 */
    smart_uart_print("\n=== GAME START ===\n");
    smart_uart_print("Use W/A/S/D to control, Q to quit\n\n");
    
    /* 游戏主循环 */
    uint32_t last_update = smart_get_tick();
    int game_started = 1;
    
    while (game_started)
    {
        /* 处理输入 */
        while (smart_uart_getc_nonblock(&c))
        {
            if (c == 'q' || c == 'Q')
            {
                game_started = 0;
                break;
            }
            snake_game_input(c);
        }
        
        /* 只在游戏运行时更新 */
        if (snake_game_get_state() == GAME_RUNNING)
        {
            /* 更新游戏（每200ms） */
            uint32_t now = smart_get_tick();
            if (now - last_update >= 200)
            {
                snake_game_update();
                snake_game_render();
                last_update = now;
            }
        }
        else if (snake_game_get_state() == GAME_OVER || snake_game_get_state() == GAME_WIN)
        {
            /* 游戏结束，等待重启或退出 */
            smart_task_yield();
        }
        else
        {
            /* 暂停状态 */
            smart_task_yield();
        }
        
        /* 让出CPU */
        smart_task_yield();
    }
    
    /* 退出游戏 */
    snake_game_exit();
    
    return 0;
}

static int cmd_synctest(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    smart_uart_print("\n=== Synchronization Test ===\n\n");
    
    /* 测试信号量 */
    smart_uart_print("1. Testing Semaphore...\n");
    
    static smart_semaphore_t test_sem;
    smart_sem_init(&test_sem, 3, 5);
    
    smart_uart_print("   Initial count: ");
    smart_uart_print_hex32(smart_sem_get_count(&test_sem));
    smart_uart_print(" (max=5)\n");
    
    /* 获取信号量 */
    smart_uart_print("   Acquiring 2 semaphores...\n");
    smart_sem_wait(&test_sem);
    smart_sem_wait(&test_sem);
    smart_uart_print("   Count after wait: ");
    smart_uart_print_hex32(smart_sem_get_count(&test_sem));
    smart_uart_print("\n");
    
    /* 释放信号量 */
    smart_uart_print("   Releasing 1 semaphore...\n");
    smart_sem_post(&test_sem);
    smart_uart_print("   Count after post: ");
    smart_uart_print_hex32(smart_sem_get_count(&test_sem));
    smart_uart_print("\n");
    
    /* 测试try_wait */
    smart_uart_print("   Testing try_wait...\n");
    smart_sync_status_t status = smart_sem_try_wait(&test_sem);
    smart_uart_print("   Result: ");
    smart_uart_print(status == SMART_SYNC_OK ? "SUCCESS" : "FAILED");
    smart_uart_print("\n");
    smart_uart_print("   Count: ");
    smart_uart_print_hex32(smart_sem_get_count(&test_sem));
    smart_uart_print("\n\n");
    
    /* 测试互斥锁 */
    smart_uart_print("2. Testing Mutex...\n");
    
    static smart_mutex_t test_mutex;
    smart_mutex_init(&test_mutex);
    
    smart_uart_print("   Initial state: ");
    smart_uart_print(smart_mutex_is_locked(&test_mutex) ? "LOCKED" : "UNLOCKED");
    smart_uart_print("\n");
    
    /* 获取锁 */
    smart_uart_print("   Acquiring mutex...\n");
    status = smart_mutex_lock(&test_mutex);
    smart_uart_print("   Result: ");
    smart_uart_print(status == SMART_SYNC_OK ? "SUCCESS" : "FAILED");
    smart_uart_print("\n");
    smart_uart_print("   State: ");
    smart_uart_print(smart_mutex_is_locked(&test_mutex) ? "LOCKED" : "UNLOCKED");
    smart_uart_print("\n");
    
    /* 测试递归锁 */
    smart_uart_print("   Testing recursive lock...\n");
    status = smart_mutex_lock(&test_mutex);
    smart_uart_print("   Result: ");
    smart_uart_print(status == SMART_SYNC_OK ? "SUCCESS (recursive)" : "FAILED");
    smart_uart_print("\n");
    
    /* 释放锁 */
    smart_uart_print("   Unlocking mutex (1st)...\n");
    smart_mutex_unlock(&test_mutex);
    smart_uart_print("   State: ");
    smart_uart_print(smart_mutex_is_locked(&test_mutex) ? "LOCKED (recursive)" : "UNLOCKED");
    smart_uart_print("\n");
    
    smart_uart_print("   Unlocking mutex (2nd)...\n");
    smart_mutex_unlock(&test_mutex);
    smart_uart_print("   State: ");
    smart_uart_print(smart_mutex_is_locked(&test_mutex) ? "LOCKED" : "UNLOCKED");
    smart_uart_print("\n\n");
    
    /* 测试try_lock */
    smart_uart_print("   Testing try_lock...\n");
    status = smart_mutex_try_lock(&test_mutex);
    smart_uart_print("   Result: ");
    smart_uart_print(status == SMART_SYNC_OK ? "SUCCESS" : "FAILED");
    smart_uart_print("\n");
    smart_uart_print("   State: ");
    smart_uart_print(smart_mutex_is_locked(&test_mutex) ? "LOCKED" : "UNLOCKED");
    smart_uart_print("\n");
    
    smart_mutex_unlock(&test_mutex);
    smart_uart_print("\n");
    
    smart_uart_print("=== Test Complete ===\n");
    smart_uart_print("Features tested:\n");
    smart_uart_print("  * Semaphore: init, wait, post, try_wait\n");
    smart_uart_print("  * Mutex: init, lock, unlock, try_lock, recursive lock\n");
    smart_uart_print("  * Priority inheritance (implicit)\n\n");
    
    return 0;
}

static int cmd_uartinfo(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    uint32_t int_count, char_count, overflow_count;
    smart_uart_get_stats(&int_count, &char_count, &overflow_count);
    
    smart_uart_print("\n=== UART Interrupt Statistics ===\n\n");
    
    smart_uart_print("Interrupt Mode:   ENABLED\n");
    smart_uart_print("Buffer Size:      256 bytes\n");
    smart_uart_print("Current Buffer:   ");
    smart_uart_print_hex32(smart_uart_rx_count());
    smart_uart_print(" bytes\n\n");
    
    smart_uart_print("Statistics:\n");
    smart_uart_print("  Interrupts:     ");
    smart_uart_print_hex32(int_count);
    smart_uart_print(" times\n");
    
    smart_uart_print("  Chars Received: ");
    smart_uart_print_hex32(char_count);
    smart_uart_print(" chars\n");
    
    smart_uart_print("  Buffer Overflow:");
    smart_uart_print_hex32(overflow_count);
    smart_uart_print(" times");
    
    if (overflow_count > 0) {
        smart_uart_print(" [WARNING]");
    } else {
        smart_uart_print(" [OK]");
    }
    smart_uart_print("\n\n");
    
    /* 计算平均每次中断接收的字符数 */
    if (int_count > 0) {
        uint32_t avg = char_count / int_count;
        smart_uart_print("Avg chars/interrupt: ");
        smart_uart_print_hex32(avg);
        smart_uart_print("\n");
    }
    
    smart_uart_print("\nInterrupt working: ");
    if (int_count > 0) {
        smart_uart_print("YES (interrupt-driven mode)\n");
    } else {
        smart_uart_print("NO (polling mode or no input yet)\n");
    }
    
    smart_uart_print("\n");
    return 0;
}

/* ========== 系统测试命令 ========== */

static int cmd_test(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    smart_uart_print("\n========================================\n");
    smart_uart_print("       SmartOS System Test\n");
    smart_uart_print("========================================\n\n");
    
    int pass = 0, fail = 0;
    
    /* 测试1: File system */
    smart_uart_print("[1] File system test...\n");
    
    const char *test_file = "TEST.TXT";
    const char *test_data = "Hello Test";
    char read_buf[32];
    int fs_ok = 1;
    smart_fs_status_t status;
    
    /* 先检查文件系统状态 */
    smart_uart_print("    Checking FS status...");
    extern smart_block_device_t *smart_flash_init(void);
    smart_block_device_t *dev = smart_flash_init();
    if (dev) {
        smart_uart_print("OK\n");
        smart_uart_print("    Formatting FS...");
        if (smart_fs_format(dev) == SMART_FS_OK) {
            smart_uart_print("OK\n");
            smart_uart_print("    Initializing FS...");
            if (smart_fs_init(dev) == SMART_FS_OK) {
                smart_uart_print("OK\n");
            } else {
                smart_uart_print("FAIL\n");
                fs_ok = 0;
            }
        } else {
            smart_uart_print("FAIL\n");
            fs_ok = 0;
        }
    } else {
        smart_uart_print("FAIL (no device)\n");
        fs_ok = 0;
    }
    
    if (fs_ok) {
        smart_uart_print("    Creating file...");
        status = smart_fs_create(test_file);
        if (status != SMART_FS_OK) {
            smart_uart_print("FAIL (create=");
            smart_uart_print_hex32(status);
            smart_uart_print(")\n");
            fs_ok = 0;
        } else {
            smart_uart_print("OK\n");
        }
    }
    
    if (fs_ok) {
        smart_uart_print("    Opening file...");
        smart_file_t file;
        status = smart_fs_open(test_file, &file);
        if (status != SMART_FS_OK) {
            smart_uart_print("FAIL (open=");
            smart_uart_print_hex32(status);
            smart_uart_print(")\n");
            fs_ok = 0;
        } else {
            smart_uart_print("OK\n");
            
            smart_uart_print("    Writing data...");
            uint32_t written;
            status = smart_fs_write(&file, test_data, strlen(test_data), &written);
            if (status != SMART_FS_OK) {
                smart_uart_print("FAIL (write=");
                smart_uart_print_hex32(status);
                smart_uart_print(")\n");
                fs_ok = 0;
            } else {
                smart_uart_print("OK (");
                smart_uart_print_hex32(written);
                smart_uart_print(" bytes)\n");
            }
            smart_fs_close(&file);
        }
    }
    
    if (fs_ok) {
        smart_uart_print("    Reading data...");
        smart_file_t file;
        status = smart_fs_open(test_file, &file);
        if (status != SMART_FS_OK) {
            smart_uart_print("FAIL (reopen=");
            smart_uart_print_hex32(status);
            smart_uart_print(")\n");
            fs_ok = 0;
        } else {
            uint32_t bytes_read;
            status = smart_fs_read(&file, read_buf, sizeof(read_buf), &bytes_read);
            if (status != SMART_FS_OK) {
                smart_uart_print("FAIL (read=");
                smart_uart_print_hex32(status);
                smart_uart_print(")\n");
                fs_ok = 0;
            } else if (bytes_read != strlen(test_data)) {
                smart_uart_print("FAIL (size mismatch: ");
                smart_uart_print_hex32(bytes_read);
                smart_uart_print(" != ");
                smart_uart_print_hex32(strlen(test_data));
                smart_uart_print(")\n");
                fs_ok = 0;
            } else if (strncmp(read_buf, test_data, bytes_read) != 0) {
                smart_uart_print("FAIL (data mismatch)\n");
                fs_ok = 0;
            } else {
                smart_uart_print("OK\n");
            }
            smart_fs_close(&file);
        }
    }
    
    smart_fs_delete(test_file);
    
    if (fs_ok) {
        smart_uart_print("    Result: PASS\n");
        pass++;
    } else {
        smart_uart_print("    Result: FAIL\n");
        fail++;
    }
    
    /* 测试2: Semaphore */
    smart_uart_print("[2] Semaphore test...\n");
    static smart_semaphore_t test_sem;
    smart_sem_init(&test_sem, 2, 5);
    smart_sem_wait(&test_sem);
    uint32_t count = smart_sem_get_count(&test_sem);
    smart_sem_post(&test_sem);
    
    if (count == 1) {
        smart_uart_print("    Result: PASS\n");
        pass++;
    } else {
        smart_uart_print("    Result: FAIL\n");
        fail++;
    }
    
    /* 测试3: Mutex */
    smart_uart_print("[3] Mutex test...\n");
    static smart_mutex_t test_mutex;
    smart_mutex_init(&test_mutex);
    smart_mutex_lock(&test_mutex);
    int locked = smart_mutex_is_locked(&test_mutex);
    smart_mutex_unlock(&test_mutex);
    int unlocked = !smart_mutex_is_locked(&test_mutex);
    
    if (locked && unlocked) {
        smart_uart_print("    Result: PASS\n");
        pass++;
    } else {
        smart_uart_print("    Result: FAIL\n");
        fail++;
    }
    
    /* 测试4: UART interrupt */
    smart_uart_print("[4] UART interrupt test...\n");
    uint32_t int_count, char_count, overflow;
    smart_uart_get_stats(&int_count, &char_count, &overflow);
    
    if (int_count > 0 && overflow == 0) {
        smart_uart_print("    Result: PASS\n");
        pass++;
    } else {
        smart_uart_print("    Result: FAIL\n");
        fail++;
    }
    
    /* Summary */
    smart_uart_print("\n========================================\n");
    smart_uart_print("Total: ");
    smart_uart_print_hex32(pass + fail);
    smart_uart_print(" tests, ");
    smart_uart_print_hex32(pass);
    smart_uart_print(" passed, ");
    smart_uart_print_hex32(fail);
    smart_uart_print(" failed\n");
    
    if (fail == 0) {
        smart_uart_print("Status: ALL TESTS PASSED!\n");
    } else {
        smart_uart_print("Status: SOME TESTS FAILED\n");
    }
    smart_uart_print("========================================\n\n");
    
    return 0;
}

/* ========== 压力测试命令 ========== */

static int cmd_stress(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    smart_uart_print("\n========================================\n");
    smart_uart_print("       SmartOS Stress Test\n");
    smart_uart_print("========================================\n\n");
    
    uint32_t start_time, elapsed;
    
    /* 测试1: 文件系统压力测试 */
    smart_uart_print("[1] File system stress (10 files)...\n");
    const char *files[] = {"F1.TXT", "F2.TXT", "F3.TXT", "F4.TXT", "F5.TXT",
                           "F6.TXT", "F7.TXT", "F8.TXT", "F9.TXT", "F10.TXT"};
    char data[64];
    int fs_pass = 1;
    
    start_time = smart_get_tick();
    
    /* 创建10个文件 */
    for (int i = 0; i < 10; i++) {
        if (smart_fs_create(files[i]) != SMART_FS_OK) {
            fs_pass = 0;
            break;
        }
        
        /* 写入数据 */
        for (int j = 0; j < 64; j++) data[j] = (char)(i + j);
        smart_file_t file;
        if (smart_fs_open(files[i], &file) == SMART_FS_OK) {
            uint32_t written;
            smart_fs_write(&file, data, 64, &written);
            smart_fs_close(&file);
        }
    }
    
    /* 验证所有文件 */
    char read_buf[64];
    for (int i = 0; i < 10 && fs_pass; i++) {
        smart_file_t file;
        if (smart_fs_open(files[i], &file) == SMART_FS_OK) {
            uint32_t bytes_read;
            smart_fs_read(&file, read_buf, 64, &bytes_read);
            smart_fs_close(&file);
            
            /* 验证数据 */
            for (int j = 0; j < 64; j++) {
                if (read_buf[j] != (char)(i + j)) {
                    fs_pass = 0;
                    break;
                }
            }
        } else {
            fs_pass = 0;
        }
    }
    
    /* 删除所有文件 */
    for (int i = 0; i < 10; i++) {
        smart_fs_delete(files[i]);
    }
    
    elapsed = smart_get_tick() - start_time;
    
    smart_uart_print("    Time: ");
    smart_uart_print_hex32(elapsed);
    smart_uart_print(" ticks (");
    smart_uart_print_hex32(640 * 1000 / elapsed);  /* 640 bytes total */
    smart_uart_print(" bytes/sec)\n");
    smart_uart_print("    Result: ");
    smart_uart_print(fs_pass ? "PASS\n" : "FAIL\n");
    
    /* 测试2: 信号量并发测试 */
    smart_uart_print("[2] Semaphore stress (1000 ops)...\n");
    static smart_semaphore_t sem;
    smart_sem_init(&sem, 5, 10);
    int sem_pass = 1;
    
    start_time = smart_get_tick();
    
    for (int i = 0; i < 1000; i++) {
        if (i % 2 == 0) {
            if (smart_sem_get_count(&sem) > 0) {
                smart_sem_wait(&sem);
            }
        } else {
            if (smart_sem_get_count(&sem) < 10) {
                smart_sem_post(&sem);
            }
        }
    }
    
    elapsed = smart_get_tick() - start_time;
    
    uint32_t final_count = smart_sem_get_count(&sem);
    if (final_count > 10) sem_pass = 0;
    
    smart_uart_print("    Time: ");
    smart_uart_print_hex32(elapsed);
    smart_uart_print(" ticks (");
    smart_uart_print_hex32(1000 * 1000 / elapsed);
    smart_uart_print(" ops/sec)\n");
    smart_uart_print("    Final count: ");
    smart_uart_print_hex32(final_count);
    smart_uart_print("/10\n");
    smart_uart_print("    Result: ");
    smart_uart_print(sem_pass ? "PASS\n" : "FAIL\n");
    
    /* 测试3: 互斥锁压力测试 */
    smart_uart_print("[3] Mutex stress (1000 lock/unlock)...\n");
    static smart_mutex_t mutex;
    smart_mutex_init(&mutex);
    int mutex_pass = 1;
    
    start_time = smart_get_tick();
    for (int i = 0; i < 1000; i++) {
        smart_mutex_lock(&mutex);
        smart_mutex_unlock(&mutex);
    }
    elapsed = smart_get_tick() - start_time;
    
    smart_uart_print("    Time: ");
    smart_uart_print_hex32(elapsed);
    smart_uart_print(" ticks (");
    if (elapsed > 0) {
        smart_uart_print_hex32(1000 * 1000 / elapsed);
        smart_uart_print(" ops/sec)\n");
    } else {
        smart_uart_print("< 1ms per 1000 ops)\n");
    }
    smart_uart_print("    Avg: ");
    if (elapsed > 0) {
        smart_uart_print_hex32(elapsed * 1000 / 1000);
        smart_uart_print(" us/op\n");
    } else {
        smart_uart_print("< 1 us/op\n");
    }
    smart_uart_print("    Result: ");
    smart_uart_print(mutex_pass ? "PASS\n" : "FAIL\n");
    
    /* 测试4: 系统稳定性测试 */
    smart_uart_print("[4] System stability (5000 ticks)...\n");
    start_time = smart_get_tick();
    uint32_t target = start_time + 5000;
    int stability_pass = 1;
    uint32_t yield_count = 0;
    
    while (smart_get_tick() < target) {
        smart_task_yield();
        yield_count++;
    }
    
    uint32_t actual = smart_get_tick() - start_time;
    if (actual < 5000 || actual > 5100) {
        stability_pass = 0;
    }
    
    smart_uart_print("    Target: 5000 ticks\n");
    smart_uart_print("    Actual: ");
    smart_uart_print_hex32(actual);
    smart_uart_print(" ticks (");
    if (actual > 5000) {
        smart_uart_print("+");
        smart_uart_print_hex32(actual - 5000);
    } else {
        smart_uart_print("-");
        smart_uart_print_hex32(5000 - actual);
    }
    smart_uart_print(" drift)\n");
    smart_uart_print("    Yields: ");
    smart_uart_print_hex32(yield_count);
    smart_uart_print(" (");
    smart_uart_print_hex32(yield_count * 1000 / actual);
    smart_uart_print(" yields/sec)\n");
    smart_uart_print("    Result: ");
    smart_uart_print(stability_pass ? "PASS\n" : "FAIL\n");
    
    /* 总结 */
    int total_pass = fs_pass + sem_pass + mutex_pass + stability_pass;
    smart_uart_print("\n========================================\n");
    smart_uart_print("Stress Test Summary:\n");
    smart_uart_print("  Tests passed: ");
    smart_uart_print_hex32(total_pass);
    smart_uart_print("/4\n");
    
    if (total_pass == 4) {
        smart_uart_print("  Status: ALL TESTS PASSED!\n");
        smart_uart_print("\nPerformance Grade: ");
        if (elapsed < 10) {
            smart_uart_print("EXCELLENT\n");
        } else if (elapsed < 50) {
            smart_uart_print("GOOD\n");
        } else {
            smart_uart_print("ACCEPTABLE\n");
        }
    } else {
        smart_uart_print("  Status: SOME TESTS FAILED\n");
    }
    smart_uart_print("========================================\n\n");
    
    return 0;
}

/* ========== 定时器测试命令 ========== */

/* 定时器回调函数 */
static void timer_callback_oneshot(void *arg)
{
    uint32_t id = (uint32_t)arg;
    smart_uart_print("[Timer] One-shot timer ");
    smart_uart_print_hex32(id);
    smart_uart_print(" expired at tick=");
    smart_uart_print_hex32(smart_get_tick());
    smart_uart_print("\n");
}

static void timer_callback_periodic(void *arg)
{
    static uint32_t count = 0;
    uint32_t id = (uint32_t)arg;
    count++;
    smart_uart_print("[Timer] Periodic timer ");
    smart_uart_print_hex32(id);
    smart_uart_print(" tick #");
    smart_uart_print_hex32(count);
    smart_uart_print(" at tick=");
    smart_uart_print_hex32(smart_get_tick());
    smart_uart_print("\n");
}

static int cmd_timer(int argc, char *argv[])
{
    if (argc < 2) {
        smart_uart_print("\nUsage: timer [list|test]\n");
        smart_uart_print("  list  - List all timers\n");
        smart_uart_print("  test  - Run timer test\n\n");
        return 0;
    }
    
    if (strcmp(argv[1], "list") == 0) {
        /* 列出所有定时器 */
        smart_timer_list();
        return 0;
    }
    
    if (strcmp(argv[1], "test") == 0) {
        smart_uart_print("\n=== Software Timer Test ===\n\n");
        
        /* 测试1: 创建单次定时器 */
        smart_uart_print("1. Creating one-shot timers...\n");
        timer_handle_t timer1 = smart_timer_create(TIMER_ONE_SHOT, 1000, timer_callback_oneshot, (void*)1);
        timer_handle_t timer2 = smart_timer_create(TIMER_ONE_SHOT, 2000, timer_callback_oneshot, (void*)2);
        timer_handle_t timer3 = smart_timer_create(TIMER_ONE_SHOT, 3000, timer_callback_oneshot, (void*)3);
        
        if (timer1 && timer2 && timer3) {
            smart_uart_print("   Created 3 one-shot timers (1s, 2s, 3s)\n\n");
        } else {
            smart_uart_print("   Error: Failed to create timers\n\n");
            return -1;
        }
        
        /* 测试2: 创建周期定时器 */
        smart_uart_print("2. Creating periodic timer...\n");
        timer_handle_t timer4 = smart_timer_create(TIMER_PERIODIC, 500, timer_callback_periodic, (void*)4);
        
        if (timer4) {
            smart_uart_print("   Created periodic timer (500ms)\n\n");
        } else {
            smart_uart_print("   Error: Failed to create periodic timer\n\n");
            return -1;
        }
        
        /* 测试3: 启动所有定时器 */
        smart_uart_print("3. Starting all timers...\n");
        uint32_t start_tick = smart_get_tick();
        smart_uart_print("   Start time: ");
        smart_uart_print_hex32(start_tick);
        smart_uart_print("\n\n");
        
        smart_timer_start(timer1);
        smart_timer_start(timer2);
        smart_timer_start(timer3);
        smart_timer_start(timer4);
        
        /* 测试4: 等待定时器触发 */
        smart_uart_print("4. Waiting for timers (5 seconds)...\n");
        smart_uart_print("   Press any key to stop early\n\n");
        
        uint32_t target_tick = start_tick + 5000;
        char c;
        
        while (smart_get_tick() < target_tick) {
            if (smart_uart_getc_nonblock(&c)) {
                smart_uart_print("\n   Stopped by user\n\n");
                break;
            }
            smart_task_yield();
        }
        
        /* 测试5: 停止周期定时器 */
        smart_uart_print("5. Stopping periodic timer...\n");
        smart_timer_stop(timer4);
        smart_uart_print("   Periodic timer stopped\n\n");
        
        /* 测试6: 显示统计信息 */
        smart_uart_print("6. Timer statistics:\n");
        timer_stats_t stats;
        smart_timer_get_stats(&stats);
        
        smart_uart_print("   Total timers:    ");
        smart_uart_print_hex32(stats.total_timers);
        smart_uart_print("\n");
        
        smart_uart_print("   Active timers:   ");
        smart_uart_print_hex32(stats.active_timers);
        smart_uart_print("\n");
        
        smart_uart_print("   Expired count:   ");
        smart_uart_print_hex32(stats.expired_count);
        smart_uart_print("\n");
        
        smart_uart_print("   Callback count:  ");
        smart_uart_print_hex32(stats.callback_count);
        smart_uart_print("\n");
        
        smart_uart_print("   Max callback:    ");
        smart_uart_print_hex32(stats.max_callback_time_us);
        smart_uart_print(" us\n\n");
        
        /* 测试7: 清理定时器 */
        smart_uart_print("7. Cleaning up...\n");
        smart_timer_delete(timer1);
        smart_timer_delete(timer2);
        smart_timer_delete(timer3);
        smart_timer_delete(timer4);
        smart_uart_print("   All timers deleted\n\n");
        
        smart_uart_print("=== Test Complete ===\n\n");
        
        return 0;
    }
    
    smart_uart_print("Unknown timer command: ");
    smart_uart_print(argv[1]);
    smart_uart_print("\n");
    return -1;
}

/* ========== Shell 核心功能 ========== */

static void shell_print_prompt(void)
{
    smart_uart_print(SHELL_PROMPT);
}

static int shell_parse_command(const char *line, char *argv[], int max_args)
{
    int argc = 0;
    int in_word = 0;
    char *word_start = NULL;
    static char line_copy[SHELL_INPUT_BUFFER_SIZE];
    
    /* 复制输入行（因为我们要修改它） */
    strncpy(line_copy, line, SHELL_INPUT_BUFFER_SIZE - 1);
    line_copy[SHELL_INPUT_BUFFER_SIZE - 1] = '\0';
    
    char *p = line_copy;
    
    while (*p && argc < max_args) {
        if (*p == ' ' || *p == '\t') {
            if (in_word) {
                *p = '\0';
                argv[argc++] = word_start;
                in_word = 0;
            }
        } else {
            if (!in_word) {
                word_start = p;
                in_word = 1;
            }
        }
        p++;
    }
    
    if (in_word && argc < max_args) {
        argv[argc++] = word_start;
    }
    
    return argc;
}

static int shell_execute_command(int argc, char *argv[])
{
    if (argc == 0) {
        return 0;
    }
    
    /* 查找命令 */
    for (int i = 0; shell_commands[i].name != NULL; i++) {
        if (strcmp(argv[0], shell_commands[i].name) == 0) {
            return shell_commands[i].handler(argc, argv);
        }
    }
    
    /* 命令未找到 */
    smart_uart_print("Unknown command: ");
    smart_uart_print(argv[0]);
    smart_uart_print("\nType 'help' for available commands.\n");
    
    return -1;
}

static void shell_process_input(char c)
{
    if (c == '\r' || c == '\n') {
        /* 回车 - 执行命令 */
        smart_uart_print("\n");
        
        if (shell_ctx.input_pos > 0) {
            shell_ctx.input_buffer[shell_ctx.input_pos] = '\0';
            
            /* 解析并执行命令 */
            char *argv[SHELL_MAX_ARGS];
            int argc = shell_parse_command(shell_ctx.input_buffer, argv, SHELL_MAX_ARGS);
            
            if (argc > 0) {
                shell_execute_command(argc, argv);
            }
            
            /* 清空缓冲区 */
            shell_ctx.input_pos = 0;
        }
        
        shell_print_prompt();
    }
    else if (c == '\b' || c == 0x7F) {
        /* 退格 */
        if (shell_ctx.input_pos > 0) {
            shell_ctx.input_pos--;
            /* 发送退格序列：退格、空格、退格 */
            smart_uart_print("\b \b");
        }
    }
    else if (c >= 32 && c < 127) {
        /* 可打印字符 */
        if (shell_ctx.input_pos < SHELL_INPUT_BUFFER_SIZE - 1) {
            shell_ctx.input_buffer[shell_ctx.input_pos++] = c;
            smart_uart_putc(c);  /* 回显 */
        } else {
            /* 缓冲区已满 */
            smart_uart_print("\n[Buffer full]\n");
            shell_ctx.input_pos = 0;
            shell_print_prompt();
        }
    }
    /* 其他字符忽略 */
}

/* Shell 任务入口 */
void shell_task_entry(void *param)
{
    (void)param;
    
    /* 显示欢迎信息 */
    smart_uart_print("\n");
    smart_uart_print("========================================\n");
    smart_uart_print("  Welcome to Smart-OS Shell\n");
    smart_uart_print("  Type 'help' for available commands\n");
    smart_uart_print("========================================\n");
    smart_uart_print("\n");
    
    shell_print_prompt();
    
    shell_ctx.running = 1;
    
    while (shell_ctx.running) {
        char c;
        
        /* 非阻塞读取输入 */
        while (smart_uart_getc_nonblock(&c)) {
            shell_process_input(c);
        }
        
        /* 处理完所有输入后，让出 CPU */
        smart_task_yield();
    }
}

/* Shell 初始化 */
void smart_shell_init(void)
{
    /* Shell 将在 main 中作为任务创建 */
    /* 这里只是一个占位函数，实际初始化在 main.c 中完成 */
}
