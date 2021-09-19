#ifndef KERNEL_KMI_H
#define KERNEL_KMI_H

/**
 * @file kernel/kmi.h
 * 
 * Keyboard input code.
 * 
 * PBX-A9 has two KMIs: KMI0 is used for keyboard input and KMI1 is used for
 * mouse input. 
 * 
 * For more info, see ARM PrimeCell PS2 Keyboard/Mouse Interface (PL050)
 * Technical Reference Manual.
 */

#define KMI0_BASE         0x10006000    ///< KMI0 (keyboard) base address         

/** @name KMIRegs
 *  KMI registers, shifted right by 2 bits for use as uint32_t[] indices
 */
///@{
#define KMICR             (0x00 >> 2)   ///< Control register
#define   KMICR_RXINTREN    (1 << 4)    ///< Enable receiver interrupt
#define KMISTAT           (0x04 >> 2)   ///< Status register
#define   KMISTAT_RXFULL    (1 << 4)    ///< Receiver register full
#define   KMISTAT_TXEMPTY   (1 << 6)    ///< Transmit register empty
#define KMIDATA           (0x08 >> 2)   ///< Received data
///@}

void kmi_kbd_init(void);
void kmi_kbd_intr(void);

#endif  // !KERNEL_KMI_H
