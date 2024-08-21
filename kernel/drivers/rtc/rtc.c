#include <stdint.h>
#include <string.h>

#include <kernel/drivers/rtc.h>
#include <kernel/mach.h>

/**
 * Initialize the RTC driver.
 */
void
rtc_init(void)
{
  mach_current->rtc_init();
}

/**
 * Get the current UTC time.
 * 
 * @return The current UTC time.
 */
time_t
rtc_get_time(void)
{
  return mach_current->rtc_get_time();
}

/**
 * Set the current UTC time.
 * 
 * @param time  New time value.
 */
void
rtc_set_time(time_t time)
{
  mach_current->rtc_set_time(time);
}
