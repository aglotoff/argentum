#ifndef __TIME_H__
#define __TIME_H__

#include <stddef.h>
#include <sys/types.h>

/**
 * Structure representing a calendar date and time.
 */
struct tm {
  /** Seconds [0,60] */
  int tm_sec;
  /** Minutes [0,59] */
  int tm_min;
  /** Hour [0,23] */
  int tm_hour;
  /** Day of month [1,31] */
  int tm_mday; 
  /** Month of year [0,11] */
  int tm_mon;  
  /** Years since 1900. */
  int tm_year; 
  /** Day of week [0,6] (Sunday = 0) */
  int tm_wday; 
  /** Day of year [0,365] */
  int tm_yday; 
  /** Daylight Savings Flag */
  int tm_isdst;
};

/**
 * Structure represnting a time interval broken into seconds and nanoseconds.
 */
struct timespec {
  /** Seconds */
  time_t tv_sec;
  /** Nanoseconds */
  long   tv_nsec;
};

enum {
  /** The identifier of the system-wide realtime clock */
  CLOCK_REALTIME  = 0,
  /** The identifier for the system-wide monotonic clock */
  CLOCK_MONOTONIC = 1,
};

char       *asctime(const struct tm *);
struct tm  *gmtime(const time_t *);
time_t      mktime(struct tm *);
size_t      strftime(char *, size_t, const char *, const struct tm *);
time_t      time(time_t *);
int         clock_gettime(clockid_t, struct timespec *);

#endif  // !__TIME_H__
