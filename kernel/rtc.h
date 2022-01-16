#ifndef __KERNEL_RTC_H__
#define __KERNEL_RTC_H__

/**
 * @file kernel/rtc.h
 */

#include <time.h>

void   rtc_init(void);
time_t rtc_time(void);

#endif  // !__KERNEL_RTC_H__
