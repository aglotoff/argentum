#include <sys/time.h>
#include <stdio.h>
#include <sys/syscall.h>

int
select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds,
       struct timeval *timeout)
{
  return __syscall5(__SYS_SELECT, nfds, readfds, writefds, errorfds, timeout);
}
