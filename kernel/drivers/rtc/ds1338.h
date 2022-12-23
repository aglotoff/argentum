#ifndef __KERNEL_DRIVERS_RTC_DS1338_H__
#define __KERNEL_DRIVERS_RTC_DS1338_H__

/**
 * @file kernel/drivers/rtc/ds1338.h
 *
 * DS1338 RTC driver.
 */

#include <stdint.h>
#include <time.h>

#include "sbcon.h"

struct DS1338 {
  struct SBCon *i2c;
  uint8_t       address;
};

void ds1338_init(struct DS1338 *, struct SBCon *, uint8_t);
void ds1338_get_time(struct DS1338 *, struct tm *);

#endif  // !__KERNEL_DRIVERS_RTC_DS1338_H__
