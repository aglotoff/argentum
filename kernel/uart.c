#include <stdint.h>

#include "console.h"
#include "uart.h"

static volatile uint32_t *uart;

void
uart_init(void)
{
  uint32_t divisor_x_64;

  // TODO: map to a reserved region in the virtual address space.
  uart = (volatile uint32_t *) UART0;

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
  // Check whether the receive FIFO is empty.
  if (uart[UARTFR] & UARTFR_RXFE)
    return -1;

  return uart[UARTDR] & 0xFF;
}

void
uart_intr(void)
{
  // Store the available data in the console buffer.
  console_intr(uart_getc);
}
