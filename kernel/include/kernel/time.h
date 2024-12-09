#ifndef __KERNEL_INCLUDE_KERNEL_TIME_H__
#define __KERNEL_INCLUDE_KERNEL_TIME_H__

#include <sys/types.h>

/** The number of ticks per one second */
#define TICKS_PER_SECOND    100
/** The number of milliseconds in one tick */
#define MS_PER_TICK         10
/** The number of microseconds in one tick */
#define US_PER_TICK         10000
/** The number of nanoseconds in one tick */
#define NS_PER_TICK         10000000

void   arch_time_init(void);
time_t arch_get_time_seconds(void);

time_t time_get_seconds(void);
void   time_init(void);
void   time_tick(void);
int    time_get(clockid_t, struct timespec *);
int    time_nanosleep(struct timespec *, struct timespec *);

static inline unsigned long long
ms2ticks(unsigned long long ms)
{
  return ms / MS_PER_TICK;
}

static inline unsigned long long
ticks2ms(unsigned long long ticks)
{
  return ticks * MS_PER_TICK;
}

static inline unsigned long long
timeval2ticks(const struct timeval *tv)
{
  return tv->tv_sec * TICKS_PER_SECOND + tv->tv_usec / US_PER_TICK;
}

static inline unsigned long long
seconds2ticks(unsigned long long seconds)
{
  return seconds * TICKS_PER_SECOND;
}

static inline unsigned long long
ticks2seconds(unsigned long long ticks)
{
  return ticks / TICKS_PER_SECOND;
}

static inline void
ticks2timespec(unsigned long long ticks, struct timespec *ts)
{
  ts->tv_sec  = ticks / TICKS_PER_SECOND;
  ts->tv_nsec = (ticks % TICKS_PER_SECOND) * NS_PER_TICK;
}

static inline unsigned long long
timespec2ticks(const struct timespec *ts)
{
  return ts->tv_sec * TICKS_PER_SECOND + ts->tv_nsec / NS_PER_TICK;
}

#endif  // !__KERNEL_INCLUDE_KERNEL_TIME_H__
