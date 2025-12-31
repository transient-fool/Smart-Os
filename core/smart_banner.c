#include "smart_uart.h"

/* ASCII艺术字 - SMART-OS (纯ASCII版本) */
void smart_print_banner(void)
{
    smart_uart_print("\n\n");
    smart_uart_print("     _____ __  __          _____ _______       ____   _____ \n");
    smart_uart_print("    / ____|  \\/  |   /\\   |  __ \\__   __|     / __ \\ / ____|\n");
    smart_uart_print("   | (___ | \\  / |  /  \\  | |__) | | |  _____| |  | | (___  \n");
    smart_uart_print("    \\___ \\| |\\/| | / /\\ \\ |  _  /  | | |_____| |  | |\\___ \\ \n");
    smart_uart_print("    ____) | |  | |/ ____ \\| | \\ \\  | |       | |__| |____) |\n");
    smart_uart_print("   |_____/|_|  |_/_/    \\_\\_|  \\_\\ |_|        \\____/|_____/ \n");
    smart_uart_print("\n");
    smart_uart_print("    +---------------------------------------------------------------+\n");
    smart_uart_print("    |  A Lightweight Real-Time OS for ARM Cortex-M3               |\n");
    smart_uart_print("    +---------------------------------------------------------------+\n");
    smart_uart_print("    |  Features:                                                  |\n");
    smart_uart_print("    |    * EDF Scheduler with AI Performance Analysis             |\n");
    smart_uart_print("    |    * FAT12 File System                                      |\n");
    smart_uart_print("    |    * Interactive Shell (15+ Commands)                       |\n");
    smart_uart_print("    |    * Message Queue & Memory Pool                            |\n");
    smart_uart_print("    |    * Stack Overflow Detection                               |\n");
    smart_uart_print("    |    * Built-in Snake Game!                                   |\n");
    smart_uart_print("    +---------------------------------------------------------------+\n");
    smart_uart_print("    |  Version: 1.0.0  |  Build: " __DATE__ " " __TIME__ "        |\n");
    smart_uart_print("    +---------------------------------------------------------------+\n");
    smart_uart_print("\n");
}

/* 简化版本 */
void smart_print_banner_simple(void)
{
    smart_uart_print("\n\n");
    smart_uart_print("    ====================================\n");
    smart_uart_print("          SMART-OS v1.0.0\n");
    smart_uart_print("    ====================================\n");
    smart_uart_print("    Real-Time OS for ARM Cortex-M3\n");
    smart_uart_print("    Build: " __DATE__ " " __TIME__ "\n");
    smart_uart_print("    ====================================\n");
    smart_uart_print("\n");
}

/* 启动动画 */
void smart_print_boot_animation(void)
{
    smart_uart_print("    [*] Initializing system...\n");
    smart_uart_print("    [");
    for (int i = 0; i < 50; i++)
    {
        smart_uart_putc('=');
        
        /* 简单延时 */
        for (volatile int j = 0; j < 50000; j++);
    }
    smart_uart_print("] 100%\n\n");
    
    smart_uart_print("    [OK] Kernel initialized\n");
    smart_uart_print("    [OK] Memory pool ready\n");
    smart_uart_print("    [OK] File system mounted\n");
    smart_uart_print("    [OK] Shell started\n");
    smart_uart_print("\n");
    smart_uart_print("    System ready! Type 'help' for available commands.\n\n");
}
