#ifndef __KERNEL_DRIVERS_CONSOLE_PL011_H__
#define __KERNEL_DRIVERS_CONSOLE_PL011_H__

/**
 * @file kernel/drivers/console/pl011.h
 * 
 * PrimeCell UART (PL011) driver.
 */

#include <stdint.h>
#include <kernel/drivers/uart.h>

/**
 * PL011 Driver instance.
 */
struct Pl011 {
  volatile uint32_t *base;    ///< Memory base address
};

int  pl011_init(struct Pl011 *, void *, unsigned long, unsigned long);

extern struct UartOps pl011_ops;

#endif  // !__KERNEL_DRIVERS_CONSOLE_PL011_H__
