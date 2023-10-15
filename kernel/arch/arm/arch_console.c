#include <arch_console.h>
#include <pl011.h>
#include <realview_bpx_a9.h>

#define UART_BAUD_RATE    115200        // Required baud rate for UART

static struct PL011 uart0;

void
arch_console_init(void)
{
  pl011_init(&uart0, (void *) UART0_BASE, UART_CLOCK, UART_BAUD_RATE);
}

void
arch_console_putc(char c)
{
  pl011_tx(&uart0, c);
}

int
arch_console_getc(void)
{
  return pl011_rx(&uart0);
}
