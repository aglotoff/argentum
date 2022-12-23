#ifndef __KERNEL_DRIVERS_RTC_SBCON_H__
#define __KERNEL_DRIVERS_RTC_SBCON_H__

/**
 * @file kernel/drivers/rtc/sbcon.h
 *
 * Two-wire serial bus interface driver.
 */

#include <stdint.h>

struct SBCon {
  volatile uint32_t *base;
};

void sbcon_init(struct SBCon *, void *);
int  sbcon_read(struct SBCon *, uint8_t, uint8_t);

#endif  // !__KERNEL_DRIVERS_SBCON_H__
