#include <stdint.h>
#include <stdio.h>

#include "console.h"

/*
 * UART I/O.
 *
 * See PrimeCell UART (PL011) Technical Reference Manual
 */

#define UART0           0x10009000    // UART0 memory base address
#define UARTDR          (0x000 >> 2)  // Data Register
#define UARTFR          (0x018 >> 2)  // Flag Register
#define   UARTFR_RXFE   (1 << 4)      // Receive FIFO empty
#define   UARTFR_TXFF   (1 << 5)      // Transmit FIFO full

static volatile uint32_t *uart = (volatile uint32_t *) UART0;

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
