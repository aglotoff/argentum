#ifndef __KERNEL_DRIVERS_UART_H__
#define __KERNEL_DRIVERS_UART_H__

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

struct UartOps {
  int (*read)(void *);
  int (*write)(void *, int);
};

struct Uart {
  struct UartOps *ops;
  void *ctx;
};

int uart_init(struct Uart *, struct UartOps *, void *, int);
int uart_getc(struct Uart *);
int uart_putc(struct Uart *, int);

#endif  // !__KERNEL_DRIVERS_UART_H__
