#include "smart_uart.h"
#include "smart_core.h"

/* UART0 寄存器 (LM3S 系列) */
#define UART0_DR    (*(volatile uint32_t *)0x4000C000)
#define UART0_FR    (*(volatile uint32_t *)0x4000C018)
#define UART0_IBRD  (*(volatile uint32_t *)0x4000C024)
#define UART0_FBRD  (*(volatile uint32_t *)0x4000C028)
#define UART0_LCRH  (*(volatile uint32_t *)0x4000C02C)
#define UART0_CTL   (*(volatile uint32_t *)0x4000C030)
#define UART0_IM    (*(volatile uint32_t *)0x4000C038)  /* 中断屏蔽 */
#define UART0_RIS   (*(volatile uint32_t *)0x4000C03C)  /* 原始中断状态 */
#define UART0_MIS   (*(volatile uint32_t *)0x4000C040)  /* 屏蔽后中断状态 */
#define UART0_ICR   (*(volatile uint32_t *)0x4000C044)  /* 中断清除 */

/* 系统控制寄存器 (LM3S811 - Sandstorm/Fury Class) */
#define SYSCTL_RCGC1        (*(volatile uint32_t *)0x400FE104)
#define SYSCTL_RCGC2        (*(volatile uint32_t *)0x400FE108)

/* NVIC寄存器 (Nested Vectored Interrupt Controller) */
#define NVIC_EN0    (*(volatile uint32_t *)0xE000E100)  /* 中断使能 0-31 */
#define NVIC_PRI1   (*(volatile uint32_t *)0xE000E404)  /* 优先级 4-7 */

/* 接收缓冲区（环形缓冲区） */
#define RX_BUFFER_SIZE 256
static volatile char rx_buffer[RX_BUFFER_SIZE];
static volatile uint32_t rx_head = 0;  /* 写入位置 */
static volatile uint32_t rx_tail = 0;  /* 读取位置 */
static volatile uint32_t rx_count = 0; /* 缓冲区数据量 */

/* 中断统计信息 */
static volatile uint32_t rx_interrupt_count = 0;  /* 中断触发次数 */
static volatile uint32_t rx_char_count = 0;       /* 接收字符总数 */
static volatile uint32_t rx_overflow_count = 0;   /* 缓冲区溢出次数 */

static int uart_initialized = 0;

void smart_uart_init(void)
{
    if (uart_initialized)
    {
        return;
    }

    /* 1. 使能 UART0 (RCGC1 bit 0) */
    SYSCTL_RCGC1 |= 0x01;

    /* 2. 使能 GPIO Port A (RCGC2 bit 0) */
    SYSCTL_RCGC2 |= 0x01;

    /* 简单的延时等待时钟稳定 */
    volatile int i;
    for(i=0; i<100; i++);

    /* 3. 禁用 UART0 以便配置 */
    UART0_CTL &= ~0x01;

    /* 4. 设置波特率 115200 (假设系统时钟 12MHz) */
    UART0_IBRD = 6;
    UART0_FBRD = 33;

    /* 5. 配置线路控制: 8位数据, 无校验, 1停止位, 启用 FIFO */
    UART0_LCRH = 0x70;

    /* 6. 清除所有中断 */
    UART0_ICR = 0x7FF;

    /* 7. 使能接收中断和接收超时中断 */
    UART0_IM = (1 << 4) | (1 << 6);  /* RXIM | RTIM */

    /* 8. 在NVIC中使能UART0中断 (中断号5) */
    NVIC_EN0 |= (1 << 5);

    /* 9. 设置UART0中断优先级 (较低优先级，高于PendSV) */
    NVIC_PRI1 = (NVIC_PRI1 & 0xFFFF00FF) | (0xE0 << 8);  /* 优先级7 */

    /* 10. 启用 UART0, TXE, RXE */
    UART0_CTL |= 0x301;

    /* 初始化缓冲区和统计信息 */
    rx_head = 0;
    rx_tail = 0;
    rx_count = 0;
    rx_interrupt_count = 0;
    rx_char_count = 0;
    rx_overflow_count = 0;

    uart_initialized = 1;
}

void smart_uart_putc(char c)
{
    /* 等待发送缓冲区为空 (TXFF 位为 0) */
    while (UART0_FR & (1 << 5));
    UART0_DR = c;
}

void smart_uart_print(const char *str)
{
    while (*str) smart_uart_putc(*str++);
}

void smart_uart_print_hex32(uint32_t value)
{
    for(int i=28; i>=0; i-=4)
    {
        uint32_t nibble = (value >> i) & 0xF;
        smart_uart_putc(nibble < 10 ? '0'+nibble : 'A'+nibble-10);
    }
}


/* 检查是否有输入可用 */
int smart_uart_input_available(void)
{
    return (rx_count > 0);
}

/* 非阻塞读取一个字符 */
int smart_uart_getc_nonblock(char *c)
{
    if (!c)
    {
        return 0;
    }
    
    smart_enter_critical();
    
    /* 检查缓冲区是否有数据 */
    if (rx_count == 0)
    {
        smart_exit_critical();
        return 0;  /* 无数据 */
    }
    
    /* 从缓冲区读取数据 */
    *c = rx_buffer[rx_tail];
    rx_tail = (rx_tail + 1) % RX_BUFFER_SIZE;
    rx_count--;
    
    smart_exit_critical();
    
    return 1;  /* 成功读取 */
}

/* 获取缓冲区中的数据量 */
uint32_t smart_uart_rx_count(void)
{
    return rx_count;
}

/* 清空接收缓冲区 */
void smart_uart_rx_flush(void)
{
    smart_enter_critical();
    rx_head = 0;
    rx_tail = 0;
    rx_count = 0;
    smart_exit_critical();
}

/* 获取中断统计信息 */
void smart_uart_get_stats(uint32_t *int_count, uint32_t *char_count, uint32_t *overflow_count)
{
    if (int_count) *int_count = rx_interrupt_count;
    if (char_count) *char_count = rx_char_count;
    if (overflow_count) *overflow_count = rx_overflow_count;
}

/* UART0中断处理函数 */
void UART0_Handler(void)
{
    uint32_t status = UART0_MIS;  /* 读取中断状态 */
    
    /* 清除中断标志 */
    UART0_ICR = status;
    
    /* 处理接收中断 */
    if (status & ((1 << 4) | (1 << 6)))  /* RXMIS | RTMIS */
    {
        rx_interrupt_count++;  /* 统计中断次数 */
        
        /* 读取所有可用数据 */
        while (!(UART0_FR & (1 << 4)))  /* RXFE == 0 表示有数据 */
        {
            char ch = (char)(UART0_DR & 0xFF);
            rx_char_count++;  /* 统计接收字符数 */
            
            /* 如果缓冲区未满，存入缓冲区 */
            if (rx_count < RX_BUFFER_SIZE)
            {
                rx_buffer[rx_head] = ch;
                rx_head = (rx_head + 1) % RX_BUFFER_SIZE;
                rx_count++;
            }
            else
            {
                /* 缓冲区溢出 */
                rx_overflow_count++;
            }
        }
    }
}
