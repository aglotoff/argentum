#ifndef KERNEL_KMI_H
#define KERNEL_KMI_H

/**
 * @file kernel/kmi.h
 * 
 * Keyboard input code.
 */

// See ARM PrimeCell PS2 Keyboard/Mouse Interface (PL050) Technical Reference
// Manual.

#define KMI0              0x10006000    ///< KMI0 (keyboard) base address

// KMI registers, shifted right by 2 bits for use as uint32_t[] indicies
#define KMICR             (0x00 >> 2)   ///< Control register
#define   KMICR_RXINTREN  (1 << 4)      ///< Enable receiver interrupt
#define KMISTAT           (0x04 >> 2)   ///< Status register
#define   KMISTAT_RXFULL  (1 << 4)      ///< Receiver register full
#define KMIDATA           (0x08 >> 2)   ///< Received data

#define C(x) ((x) - '@')                ///< Key code for Ctrl+x

/**
 * Initialize the keyboard driver.
 */
void kmi_init(void);

/**
 * Handle interrupt from the keyboard.
 * 
 * Get data and store it into the console buffer.
 */
void kmi_intr(void);

#endif  // !KERNEL_KMI_H
