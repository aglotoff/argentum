#ifndef __KERNEL_DRIVERS_UART_H__
#define __KERNEL_DRIVERS_UART_H__

#include <kernel/trap.h>

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

struct Pl011;

struct Uart {
  struct Pl011 *pl011;
  struct ISRThread uart_isr;
  int irq;
};

int uart_init(struct Uart *, struct Pl011 *, int);
int uart_getc(struct Uart *);
int uart_putc(struct Uart *, int);

#endif  // !__KERNEL_DRIVERS_UART_H__
