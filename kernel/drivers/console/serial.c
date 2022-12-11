#include <argentum/drivers/console.h>
#include <argentum/drivers/gic.h>
#include <argentum/mm/memlayout.h>
#include <argentum/trap.h>

#include "pl011.h"
#include "serial.h"

#define UART_CLOCK        24000000U     // UART clock rate, in Hz
#define UART_BAUD_RATE    115200        // Required baud rate

// Use UART0 as serial debug console.
static struct Pl011 uart0;

/**
 * Initialize the serial console driver.
 */
void
serial_init(void)
{
  pl011_init(&uart0, PA2KVA(PHYS_UART0), UART_CLOCK, UART_BAUD_RATE);
  gic_enable(IRQ_PHYS_UART0, 0);
}

/**
 * Get a character from the serial console.
 * 
 * @return The next character from the serial console or -1 if no data
 *         available.
 */
int
serial_getc(void)
{
  int c;
  
  c = pl011_read(&uart0);

  switch (c) {
  case '\r':
    return '\n';
  case '\x7f':
    return '\b';
  default:
    return c;
  }
}

/**
 * Handle interrupt from the serial console.
 */
void
serial_interrupt(void)
{
  console_interrupt(serial_getc);
}

/**
 * Put a character to the serial console.
 * 
 * @param c The character to be written.
 */
void
serial_putc(char c)
{
  // Prepend '\r' to '\n'
  if (c == '\n')
    pl011_write(&uart0, '\r');

  pl011_write(&uart0, c);
}
