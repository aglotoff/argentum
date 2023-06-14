#ifndef __KERNEL_INCLUDE_KTIME_H__
#define __KERNEL_INCLUDE_KTIME_H__

unsigned long ktime_get(void);
void          ktime_tick(void);

#endif  // !__KERNEL_INCLUDE_KTIME_H__
