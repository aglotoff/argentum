#include <sys/time.h>
#include <stdio.h>
#include <sys/syscall.h>

int
select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds,
       struct timeval *timeout)
{
  return __syscall(__SYS_SELECT, nfds, (uint32_t) readfds, (uint32_t) writefds, (uint32_t) errorfds, (uint32_t) timeout, 0);
}
