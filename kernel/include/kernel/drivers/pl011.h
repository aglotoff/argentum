#ifndef __KERNEL_DRIVERS_CONSOLE_PL011_H__
#define __KERNEL_DRIVERS_CONSOLE_PL011_H__

/**
 * @file kernel/drivers/console/pl011.h
 * 
 * PrimeCell UART (PL011) driver.
 */

#include <stdint.h>

/**
 * PL011 Driver instance.
 */
struct Pl011 {
  volatile uint32_t *base;    ///< Memory base address
};

int  pl011_init(struct Pl011 *, void *, unsigned long, unsigned long);
void pl011_write(struct Pl011 *, char);
int  pl011_read(struct Pl011 *);

#endif  // !__KERNEL_DRIVERS_CONSOLE_PL011_H__
