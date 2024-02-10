#include <stdio.h>
#include <poll.h>

int
poll(struct pollfd fds[], nfds_t nfds, int timeout)
{
  (void) fds;
  (void) nfds;
  (void) timeout;

  fprintf(stderr, "TODO: poll\n");
  return -1;
}
