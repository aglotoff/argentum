#include <sys/time.h>
#include <stdio.h>

int
select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds,
       struct timeval *timeout)
{
  fprintf(stderr, "TODO: select(%d %p %p %p %p)\n", nfds, readfds, writefds,
         errorfds, timeout);
  return -1;
}
