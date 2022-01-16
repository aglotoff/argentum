// See the PrimeCell UART (PL011) Technical Reference Manual

#include <stdint.h>

#include "console.h"
#include "gic.h"
#include "memlayout.h"
#include "trap.h"
#include "uart.h"
#include "vm.h"

// UART0 memory base address
#define UART0             0x10009000    

// UART registers, divided by 4 for use as uint32_t[] indicies
#define UARTDR            (0x000 / 4)   // Data Register
#define UARTECR           (0x004 / 4)   // Error Clear Register
#define UARTFR            (0x018 / 4)   // Flag Register
#define   UARTFR_RXFE       (1U << 4)   //   Receive FIFO empty
#define   UARTFR_TXFF       (1U << 5)   //   Transmit FIFO full
#define UARTIBRD          (0x024 / 4)   // Integer Baud Rate Register
#define UARTFBRD          (0x028 / 4)   // Fractional Baud Rate Register
#define UARTLCR           (0x02C / 4)   // Line Control Register
#define   UARTLCR_FEN       (1U << 4)   //   Enable FIFOs
#define   UARTLCR_WLEN8     (3U << 5)   //   Word length = 8 bits
#define UARTCR            (0x030 / 4)   // Control Register
#define   UARTCR_UARTEN     (1U << 0)   //   UART Enable
#define   UARTCR_TXE        (1U << 8)   //   Transmit enable
#define   UARTCR_RXE        (1U << 9)   //   Receive enable
#define UARTIMSC          (0x038 / 4)   // Interrupt Mask Set/Clear Register
#define   UARTIMSC_RXIM     (1U << 4)   //   Receive interrupt mask

#define UART_CLK          24000000U     // UART clock rate, in Hz
#define BITRATE           115200        // Required baud rate

static volatile uint32_t *uart;

static int uart_getc(void);

/**
 * Initialize the UART driver.
 */
void
uart_init(void)
{
  uart = (volatile uint32_t *) KADDR(UART0);

  // Clear all errors.
  uart[UARTECR] = 0;

  // Disable UART.
  uart[UARTCR] = 0;

  // Set the baud rate.
  uart[UARTIBRD] = (UART_CLK / (16 * BITRATE)) & 0xFF;
  uart[UARTFBRD] = ((UART_CLK * 4 / BITRATE) >> 6) & 0x3F;

  // Enable FIFO, 8 data bits, 1 stop bit, parity off.
  uart[UARTLCR] = UARTLCR_FEN | UARTLCR_WLEN8;

  // Enable UART, transfer & receive.
  uart[UARTCR] = UARTCR_UARTEN | UARTCR_TXE | UARTCR_RXE;

  // Enable interupts.
  uart[UARTIMSC] |= UARTIMSC_RXIM;
  gic_enable(IRQ_UART0, 0);
}

/**
 * Output character to the UART device.
 * 
 * @param c The character to be printed.
 */
void
uart_putc(char c)
{
  if (c == '\n')
    uart_putc('\r');
  
  // Wait until FIFO is ready to transmit.
  while (uart[UARTFR] & UARTFR_TXFF)
    ;

  uart[UARTDR] = c;
}

/**
 * Handle interrupt from the UART device.
 * 
 * Get data and store it into the console buffer.
 */
void
uart_intr(void)
{
  console_intr(uart_getc);
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
