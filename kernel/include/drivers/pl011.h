#ifndef __KERNEL_DRIVERS_PL011_H__
#define __KERNEL_DRIVERS_PL011_H__

#ifndef __KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

/**
 * @file kernel/pl011.h
 * 
 * PrimeCell UART (PL011) driver.
 */

/**
 * PL011 Driver instance.
 */
struct Pl011 {
  volatile uint32_t *base;    ///< Memory base address
};

int  pl011_init(struct Pl011 *, void *, unsigned long, unsigned long);
void pl011_putc(struct Pl011 *, char);
int  pl011_getc(struct Pl011 *);

#endif  // !__KERNEL_DRIVERS_PL011_H__
