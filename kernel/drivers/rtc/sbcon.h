#ifndef __KERNEL_DRIVERS_RTC_SBCON_H__
#define __KERNEL_DRIVERS_RTC_SBCON_H__

#include <stddef.h>
#include <stdint.h>

/**
 * Two-wire serial bus interface driver instance.
 */
struct SBCon {
  volatile uint32_t *base;  // Base address
};

void sbcon_init(struct SBCon *, void *);
int  sbcon_read(struct SBCon *, uint8_t, uint8_t, void *, size_t);

#endif  // !__KERNEL_DRIVERS_SBCON_H__
