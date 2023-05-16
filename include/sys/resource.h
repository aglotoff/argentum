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

#define RLIMIT_NOFILE   0

#endif  // !__SYS_RESOURCE_H__
