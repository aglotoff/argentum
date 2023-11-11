#ifndef __AG_INCLUDE_ARCH_KERNEL_PL011_H__
#define __AG_INCLUDE_ARCH_KERNEL_PL011_H__

/**
 * 
 * PrimeCell UART (PL011) driver
 */

#include <stdint.h>

/**
 * PL011 Driver instance.
 */
struct PL011 {
  /** Base address */
  volatile uint32_t *regs;
};

int  pl011_init(struct PL011 *, void *, unsigned long, unsigned long);
void pl011_tx(struct PL011 *, char);
int  pl011_rx(struct PL011 *);

#endif  // !__AG_INCLUDE_ARCH_KERNEL_PL011_H__
