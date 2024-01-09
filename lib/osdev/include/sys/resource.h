#ifndef __SYS_RESOURCE_H__
#define __SYS_RESOURCE_H__

typedef unsigned long rlim_t;

struct rlimit {
  rlim_t rlim_cur; // the current (soft) limit
  rlim_t rlim_max; // the hard limit
};

#define RLIM_INFINITY   0
#define RLIM_SAVED_MAX  1
#define RLIM_SAVED_CUR  2

#define RLIMIT_CORE     0
#define RLIMIT_CPU      1
#define RLIMIT_DATA     2
#define RLIMIT_FSIZE    3
#define RLIMIT_NOFILE   4
#define RLIMIT_STACK    5
#define RLIMIT_AS       6

#endif  // !__SYS_RESOURCE_H__
