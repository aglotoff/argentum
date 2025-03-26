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

  long ru_maxrss;   // maximum resident set size
  long ru_ixrss;    // integral shared memory size
  long ru_idrss;    // integral unshared data size
  long ru_isrss;    // integral unshared stack size
  long ru_minflt;   // page reclaims (soft page faults)
  long ru_majflt;   // page faults (hard page faults)
  long ru_nswap;    // swaps
  long ru_inblock;  // block input operations
  long ru_oublock;  // block output operations
  long ru_msgsnd;   // IPC messages sent
  long ru_msgrcv;   // IPC messages received
  long ru_nsignals; // signals received
  long ru_nvcsw;    // voluntary context switches
  long ru_nivcsw;   // involuntary context switches
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
