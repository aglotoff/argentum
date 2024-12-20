#ifndef __KERNEL_DRIVERS_RTC_DS1338_H__
#define __KERNEL_DRIVERS_RTC_DS1338_H__

#include <stdint.h>
#include <time.h>

#include "sbcon.h"

/**
 * DS1338 RTC driver instance.
 */
struct DS1338 {
  struct SBCon *i2c;      // I2C driver instance to access the device
  uint8_t       address;  // Device address on the I2C bus
};

void ds1338_init(struct DS1338 *, struct SBCon *, uint8_t);
void ds1338_get_time(struct DS1338 *, struct tm *);
void ds1338_set_time(struct DS1338 *, const struct tm *);

#endif  // !__KERNEL_DRIVERS_RTC_DS1338_H__
