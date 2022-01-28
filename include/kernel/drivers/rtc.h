#ifndef __KERNEL_DRIVERS_RTC_H__
#define __KERNEL_DRIVERS_RTC_H__

#ifndef __KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

/**
 * @file kernel/rtc.h
 */

#include <time.h>

void   rtc_init(void);
time_t rtc_time(void);

#endif  // !__KERNEL_DRIVERS_RTC_H__
