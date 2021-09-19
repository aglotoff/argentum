#include <stdint.h>

#include "console.h"
#include "gic.h"
#include "memlayout.h"
#include "trap.h"
#include "uart.h"
#include "vm.h"

static volatile uint32_t *uart;

void
uart_init(void)
{
  uint32_t divisor_x_64;

  uart = (volatile uint32_t *) vm_map_mmio(UART0, 4096);

  // Clear all errors.
  uart[UARTECR] = 0;

  // Disable UART.
  uart[UARTCR] = 0;

  // Compute the baud rate divisor multiplied by 64 to preserve the fractional
  // part.
  divisor_x_64 = (UART_CLK * 4) / 19200;
  uart[UARTIBRD] = (divisor_x_64 >> 6) & 0xFFFF;
  uart[UARTFBRD] = divisor_x_64 & 0x3F;

  // Enable FIFO, 8 data bits, 1 stop bit, parity off.
  uart[UARTLCR] = UARTLCR_FEN | UARTLCR_WLEN8;

  // Enable UART, transfer & receive.
  uart[UARTCR] = UARTCR_UARTEN | UARTCR_TXE | UARTCR_RXE;

  // Enable interupts
  uart[UARTIMSC] |= UARTIMSC_RXIM;
  gic_enable(IRQ_UART0, 0);
}

void
uart_putc(char c)
{
  // Wait until FIFO is ready to transmit.
  while (uart[UARTFR] & UARTFR_TXFF)
    ;

  uart[UARTDR] = c;
}

static int
uart_getc(void)
{
  int c;
  
  // Check whether the receive FIFO is empty.
  if (uart[UARTFR] & UARTFR_RXFE)
    return -1;

  c = uart[UARTDR] & 0xFF;
  return c == '\r' ? '\n' : c;
}

void
uart_intr(void)
{
  // Store the available data in the console buffer.
  console_intr(uart_getc);
}
