#ifndef __INCLUDE_ARGENTUM_DRIVERS_RTC_H__
#define __INCLUDE_ARGENTUM_DRIVERS_RTC_H__

#ifndef __AG_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

/**
 * @file include/argentum/drivers/rtc.h
 */

#include <time.h>

void   rtc_init(void);
time_t rtc_time(void);

#endif  // !__INCLUDE_ARGENTUM_DRIVERS_RTC_H__
