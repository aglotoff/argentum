#include <kernel/drivers/pl011.h>
#include <kernel/drivers/uart.h>
#include <kernel/tty.h>
#include <kernel/interrupt.h>

static int uart_irq_thread(int, void *);

int
uart_init(struct Uart *uart, struct Pl011 *pl011, int irq)
{
  uart->pl011 = pl011;

  interrupt_attach_thread(irq, uart_irq_thread, uart);

  return 0;
}

int
uart_getc(struct Uart *uart)
{
  int c;
  
  c = pl011_read(uart->pl011);

  switch (c) {
  case '\x7f':
    return '\b';
  default:
    return c;
  }
}

int
uart_putc(struct Uart *uart, int c)
{
  if (c == '\n')
    pl011_write(uart->pl011, '\r');

  pl011_write(uart->pl011, c);

  return 0;
}

static int
uart_irq_thread(int irq, void *arg)
{
  struct Uart *uart = (struct Uart *) arg;

  char buf[2];
  int c;

  (void) irq;

  while ((c = uart_getc(uart)) >= 0) {
    if (c != 0) {
      buf[0] = c;
      buf[1] = '\0';
      tty_process_input(tty_system, buf);
    }
  }

  return 1;
}
