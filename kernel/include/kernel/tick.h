#ifndef __KERNEL_INCLUDE_TICK_H__
#define __KERNEL_INCLUDE_TICK_H__

/** The number of ticks per one second */
#define TICKS_PER_SECOND    100
/** The number of milliseconds in one tick */
#define MS_PER_TICK         10
/** The number of nanoseconds in one tick */
#define NS_PER_TICK         10000000

unsigned long tick_get(void);
void          tick(void);

#endif  // !__KERNEL_INCLUDE_TICK_H__
