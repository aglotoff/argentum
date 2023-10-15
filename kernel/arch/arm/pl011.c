// See the PrimeCell UART (PL011) Technical Reference Manual

#include <pl011.h>

// UART registers, divided by 4 to be used as uint32_t[] indicies
enum {
  UARTDR    = (0x000 / 4),   // Data Register
  UARTECR   = (0x004 / 4),   // Error Clear Register
  UARTFR    = (0x018 / 4),   // Flag Register
  UARTIBRD  = (0x024 / 4),   // Integer Baud Rate Register
  UARTFBRD  = (0x028 / 4),   // Fractional Baud Rate Register
  UARTLCR   = (0x02C / 4),   // Line Control Register
  UARTCR    = (0x030 / 4),   // Control Register
  UARTIMSC  = (0x038 / 4),   // Interrupt Mask Set/Clear Register
};

// Flag Register bits
enum {
  UARTFR_RXFE     = (1U << 4),  // Receive FIFO empty
  UARTFR_TXFF     = (1U << 5),  // Transmit FIFO full
};
// Line Control Register bits
enum {
  UARTLCR_FEN     = (1U << 4),  // Enable FIFOs
  UARTLCR_WLEN_8  = (3U << 5),  // Word length = 8 bits
};
// Control Register bits
enum {
  UARTCR_UARTEN   = (1U << 0),  // UART Enable
  UARTCR_TXE      = (1U << 8),  // Transmit enable
  UARTCR_RXE      = (1U << 9),  // Receive enable
};
// Interrupt Mask Set/Clear Register bits
enum {
  UARTIMSC_RXIM   = (1U << 4)   // Receive interrupt mask
};

/**
 * Initialize the PL011 driver.
 * 
 * @param pl011      Pointer to the driver instance
 * @param base       Base memory address
 * @param uart_clock Reference clock frequency
 * @param baud_rate  Required baud rate
 */
int
pl011_init(struct PL011 *pl011,
           void *base,
           unsigned long uart_clock,
           unsigned long baud_rate)
{
  pl011->regs = (volatile uint32_t *) base;

  // Disable UART during initialization
  pl011->regs[UARTCR] &= ~UARTCR_UARTEN;

  // Set the baud rate
  pl011->regs[UARTIBRD] = (uart_clock / (16 * baud_rate)) & 0xFFFF;
  pl011->regs[UARTFBRD] = ((uart_clock * 4 / baud_rate) >> 6) & 0x3F;

  // Enable FIFO, 8 data bits, one stop bit, parity off.
  pl011->regs[UARTLCR] = UARTLCR_FEN | UARTLCR_WLEN_8;

  // Clear any pending errors
  pl011->regs[UARTECR] = 0;

  // Enable UART, transfer & receive.
  pl011->regs[UARTCR] = UARTCR_UARTEN | UARTCR_TXE | UARTCR_RXE;

  // Enable interupts.
  // pl011->regs[UARTIMSC] |= UARTIMSC_RXIM;

  return 0;
}

/**
 * Write a data character to the PL011 device.
 * 
 * @param pl011 Pointer to the driver instance.
 * @param data The character to be transmitted.
 */
void
pl011_tx(struct PL011 *pl011, char data)
{ 
  // Wait if transmit FIFO is full
  while (pl011->regs[UARTFR] & UARTFR_TXFF)
    ;

  pl011->regs[UARTDR] = data;
}

/**
 * Read a data character from the PL011 device.
 * 
 * @param pl011 Pointer to the driver instance.
 * @return A data character or -1 if no data available.
 */
int
pl011_rx(struct PL011 *pl011)
{ 
  // Check whether the receive FIFO is empty
  if (pl011->regs[UARTFR] & UARTFR_RXFE)
    return -1;

  return pl011->regs[UARTDR] & 0xFF;
}
