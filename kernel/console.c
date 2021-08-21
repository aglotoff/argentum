#include <stdint.h>
#include <stdio.h>

#include "console.h"

/*
 * UART I/O.
 *
 * See PrimeCell UART (PL011) Technical Reference Manual
 */

#define UART0             0x10009000    // UART0 memory base address

#define UARTDR            (0x000 >> 2)  // Data Register
#define UARTECR           (0x004 >> 2)  // Error Clear Register
#define UARTFR            (0x018 >> 2)  // Flag Register
#define   UARTFR_RXFE     (1 << 4)      // Receive FIFO empty
#define   UARTFR_TXFF     (1 << 5)      // Transmit FIFO full
#define UARTIBRD          (0x024 >> 2)  // Integer Baud Rate Register
#define UARTFBRD          (0x028 >> 2)  // Fractional Baud Rate Register
#define UARTLCR           (0x02C >> 2)  // Line Control Register
#define   UARTLCR_FEN     (1 << 4)      // Enable FIFOs
#define   UARTLCR_WLEN8   (3 << 5)      // Word length = 8 bits
#define UARTCR            (0x030 >> 2)  // Control Register
#define   UARTCR_UARTEN   (1 << 0)      // UART Enable
#define   UARTCR_TXE      (1 << 8)      // Transmit enable
#define   UARTCR_RXE      (1 << 9)      // Receive enable

#define UART_CLK          24000000      // Clock rate
#define UART_BAUDRATE     19200         // Baud rate

static volatile uint32_t *uart;

static void
uart_init(void)
{
  uint32_t divisor_x64;

  uart = (volatile uint32_t *) UART0;

  // Clear all errors
  uart[UARTECR] = 0;

  // Disable UART
  uart[UARTCR] = 0;

  // Set the bit rate
  divisor_x64 = (UART_CLK * 4) / UART_BAUDRATE;
  uart[UARTIBRD] = (divisor_x64 >> 6) & 0xFFFF;
  uart[UARTFBRD] = divisor_x64 & 0x3F;

  // Enable FIFO, 8 data bits, 1 stop bit, parity off
  uart[UARTLCR] = UARTLCR_FEN | UARTLCR_WLEN8;

  // Enable UART, transfer & receive
  uart[UARTCR] = UARTCR_UARTEN | UARTCR_TXE | UARTCR_RXE;
}

static void
uart_putc(char c)
{
  while (uart[UARTFR] & UARTFR_TXFF)
    ;
  uart[UARTDR] = c;
}

static int
uart_getc(void)
{
  if (uart[UARTFR] & UARTFR_RXFE)
    return -1;
  return uart[UARTDR] & 0xFF;
}

/*
 * General device-independent console code.
 */

void
console_init(void)
{
  uart_init();
}

void
console_putc(char c)
{
  uart_putc(c);
}

int
console_getc(void)
{
  int c;
  
  // Poll for any pending characters.
  while ((c = uart_getc()) < 0)
    ;

  return c;
}

// Callback used by the vcprintf and cprintf functions.
static void
cputc(void *arg, int c)
{
  (void) arg;
  console_putc(c);
}

void
vcprintf(const char *format, va_list ap)
{
  xprintf(cputc, NULL, format, ap);
}

void
cprintf(const char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  vcprintf(format, ap);
  va_end(ap);
}
