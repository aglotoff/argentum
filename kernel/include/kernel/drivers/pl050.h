#ifndef __KERNEL_DRIVERS_CONSOLE_PL050_H__
#define __KERNEL_DRIVERS_CONSOLE_PL050_H__

/**
 * @file kernel/drivers/console/pl050.h
 * 
 * PrimeCell PS2 Keyboard/Mouse Interface (PL050) driver.
 */

#include <stdint.h>
#include <kernel/drivers/ps2.h>

/**
 * PL050 Driver instance.
 */
struct Pl050 {
  volatile uint32_t *base;    ///< Memory base address
  struct PS2         ps2;
};

int  pl050_init(struct Pl050 *, void *, int);
int  pl050_kbd_getc(struct Pl050 *);

#endif  // !__KERNEL_DRIVERS_CONSOLE_PL050_H__
