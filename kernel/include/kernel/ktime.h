#ifndef __KERNEL_INCLUDE_KTIME_H__
#define __KERNEL_INCLUDE_KTIME_H__

#define TICKS_PER_S   100
#define TICKS_PER_MS  10
#define NS_PER_TICK   10000

unsigned long ktime_get(void);
void          ktime_tick(void);

#endif  // !__KERNEL_INCLUDE_KTIME_H__
