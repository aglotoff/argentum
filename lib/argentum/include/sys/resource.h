#ifndef __SYS_RESOURCE_H__
#define __SYS_RESOURCE_H__

#include <sys/cdefs.h>
#include <sys/time.h>

__BEGIN_DECLS

typedef unsigned long rlim_t;

struct rlimit {
  rlim_t rlim_cur; // the current (soft) limit
  rlim_t rlim_max; // the hard limit
};

struct rusage {
  struct timeval ru_utime;
  struct timeval ru_stime;
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

#define	RUSAGE_SELF			0
#define	RUSAGE_CHILDREN -1

__END_DECLS

#endif  // !__SYS_RESOURCE_H__
