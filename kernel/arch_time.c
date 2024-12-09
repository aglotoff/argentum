#include <kernel/mach.h>
#include <kernel/time.h>

void
arch_time_init(void)
{
  mach_current->rtc_init();
}

time_t
arch_get_time_seconds(void)
{
  return mach_current->rtc_get_time();
}
