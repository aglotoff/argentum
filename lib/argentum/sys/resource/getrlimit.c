#include <errno.h>
#include <limits.h>
#include <sys/resource.h>
#include <stdio.h>

// TODO: keep in sync with memlayout!
#define RLIMIT_STACK_MAX  (4096 * 4)
#define RLIMIT_AS_MAX     (0x80000000 - 4096)

int
getrlimit(int resource, struct rlimit *rlim)
{
  switch (resource) {
  case RLIMIT_NOFILE:
    rlim->rlim_cur = OPEN_MAX;
    rlim->rlim_max = OPEN_MAX;
    break;
  case RLIMIT_STACK:
    rlim->rlim_cur = RLIMIT_STACK_MAX;
    rlim->rlim_max = RLIMIT_STACK_MAX;
    break;
  case RLIMIT_AS:
    rlim->rlim_cur = RLIMIT_AS_MAX;
    rlim->rlim_max = RLIMIT_AS_MAX;
    break;
  case RLIMIT_CORE:
  case RLIMIT_CPU:
  case RLIMIT_DATA:
  case RLIMIT_FSIZE:
    rlim->rlim_cur = RLIM_INFINITY;
    rlim->rlim_max = RLIM_INFINITY;
    break;
  default:
    errno = EINVAL;
    return -1;
  }
}
