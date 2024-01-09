#include <errno.h>
#include <limits.h>
#include <sys/resource.h>

int
getrlimit(int resource, struct rlimit *rlim)
{
  switch (resource) {
  case RLIMIT_NOFILE:
    rlim->rlim_cur = OPEN_MAX;
    rlim->rlim_max = OPEN_MAX;
    break;
  case RLIMIT_CORE:
  case RLIMIT_CPU:
  case RLIMIT_DATA:
  case RLIMIT_FSIZE:
  case RLIMIT_STACK:
  case RLIMIT_AS:
    // TODO: implement
    errno = ENOSYS;
    return -1;
  default:
    errno = EINVAL;
    return -1;
  }
}
