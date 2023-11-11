#include <kernel/vm.h>

#include <arch/kernel/console.h>
#include <arch/kernel/pl011.h>
#include <arch/kernel/realview_pbx_a9.h>

#define UART_BAUD_RATE    115200        // Required baud rate for UART

static struct PL011 uart0;

void
arch_console_init(void)
{
  pl011_init(&uart0, PA2KVA(UART0_BASE), UART_CLOCK, UART_BAUD_RATE);
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
