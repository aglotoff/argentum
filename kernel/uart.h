#ifndef KERNEL_UART_H
#define KERNEL_UART_H

/**
 * @file kernel/uart.h
 * 
 * UART input/output code.
 */

// See the PrimeCell UART (PL011) Technical Reference Manual.

#define UART0             0x10009000    ///< UART0 memory base address

// UART registers, shifted right by 2 bits for use as uint32_t[] indicies
#define UARTDR            (0x000 >> 2)  ///< Data Register
#define UARTECR           (0x004 >> 2)  ///< Error Clear Register
#define UARTFR            (0x018 >> 2)  ///< Flag Register
#define   UARTFR_RXFE     (1 << 4)      ///< Receive FIFO empty
#define   UARTFR_TXFF     (1 << 5)      ///< Transmit FIFO full
#define UARTIBRD          (0x024 >> 2)  ///< Integer Baud Rate Register
#define UARTFBRD          (0x028 >> 2)  ///< Fractional Baud Rate Register
#define UARTLCR           (0x02C >> 2)  ///< Line Control Register
#define   UARTLCR_FEN     (1 << 4)      ///< Enable FIFOs
#define   UARTLCR_WLEN8   (3 << 5)      ///< Word length = 8 bits
#define UARTCR            (0x030 >> 2)  ///< Control Register
#define   UARTCR_UARTEN   (1 << 0)      ///< UART Enable
#define   UARTCR_TXE      (1 << 8)      ///< Transmit enable
#define   UARTCR_RXE      (1 << 9)      ///< Receive enable

#define UART_CLK          24000000      ///< UART clock rate, in Hz

/**
 * Initialize the UART driver.
 */
void uart_init(void);

/**
 * Output character to the UART device.
 * 
 * @param c The character to be printed.
 */
void uart_putc(char c);

/**
 * Handle interrupt from the UART device.
 * 
 * Get data and store it into the console buffer.
 */
void uart_intr(void);

#endif  // !KERNEL_UART_H
