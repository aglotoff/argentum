#ifndef __KERNEL_DRIVERS_UART_H__
#define __KERNEL_DRIVERS_UART_H__

#ifndef __KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

/**
 * @file kernel/uart.h
 * 
 * UART input/output code.
 */

void uart_init(void);
void uart_putc(char);
void uart_intr(void);
int  uart_getc(void);

#endif  // !__KERNEL_DRIVERS_UART_H__
