#ifndef __SMART_UART_H__
#define __SMART_UART_H__

#include <stdint.h>

void smart_uart_init(void);
void smart_uart_putc(char c);
void smart_uart_print(const char *str);
void smart_uart_print_hex32(uint32_t value);

/* 非阻塞读取函数 */
int smart_uart_getc_nonblock(char *c);
int smart_uart_input_available(void);

/* 缓冲区管理函数 */
uint32_t smart_uart_rx_count(void);
void smart_uart_rx_flush(void);

/* 统计信息 */
void smart_uart_get_stats(uint32_t *int_count, uint32_t *char_count, uint32_t *overflow_count);

/* 中断处理函数 */
void UART0_Handler(void);

#endif


