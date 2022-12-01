#ifndef __KERNEL_DRIVERS_PL050_H__
#define __KERNEL_DRIVERS_PL050_H__

#ifndef __KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

/**
 * @file kernel/pl050.h
 * 
 * PrimeCell PS2 Keyboard/Mouse Interface (PL050) driver.
 */

#include <stdint.h>

/**
 * PL050 Driver instance.
 */
struct Pl050 {
  volatile uint32_t *base;    ///< Memory base address
};

int  pl050_init(struct Pl050 *, void *);
void pl050_putc(struct Pl050 *, char);
int  pl050_getc(struct Pl050 *);

#endif  // !__KERNEL_DRIVERS_PL050_H__
