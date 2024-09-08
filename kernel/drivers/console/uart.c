#include <kernel/drivers/pl011.h>
#include <kernel/drivers/uart.h>
#include <kernel/drivers/console.h>

static void uart_irq_thread(void *);

int
uart_init(struct Uart *uart, struct Pl011 *pl011, int irq)
{
  uart->pl011 = pl011;
  uart->irq = irq;

  interrupt_attach_thread(&uart->uart_isr, irq, uart_irq_thread, uart);

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

static void
uart_irq_thread(void *arg)
{
  struct Uart *uart = (struct Uart *) arg;

  char buf[2];
  int c;

  while ((c = uart_getc(uart)) >= 0) {
    if (c != 0) {
      buf[0] = c;
      buf[1] = '\0';
      tty_process_input(console_system, buf);
    }
  }

  interrupt_unmask(uart->irq);
}
