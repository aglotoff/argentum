#ifndef __SYS_TIME_H__
#define __SYS_TIME_H__

#include <sys/types.h>

struct timeval {
  time_t         tv_sec;  // Seconds. 
  suseconds_t    tv_usec; // Microseconds. 
};

int gettimeofday(struct timeval *, void *);

#endif  // !__SYS_TIME_H__
