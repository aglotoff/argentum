#ifndef __KERNEL_UART_H__
#define __KERNEL_UART_H__

/**
 * @file kernel/uart.h
 * 
 * UART input/output code.
 */

void uart_init(void);
void uart_putc(char);
void uart_intr(void);

#endif  // !__KERNEL_UART_H__
