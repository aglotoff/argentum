#ifndef __KERNEL_INCLUDE_KERNEL_DRIVERS_RTC_H__
#define __KERNEL_INCLUDE_KERNEL_DRIVERS_RTC_H__

#ifndef __OSDEV_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <time.h>

void   rtc_init(void);
time_t rtc_get_time(void);
void   rtc_set_time(time_t);

#endif  // !__KERNEL_INCLUDE_KERNEL_DRIVERS_RTC_H__
