#ifndef _POLL_H
#define _POLL_H

#include <sys/cdefs.h>

__BEGIN_DECLS

struct pollfd {
  int    fd;
  short  events;
  short  revents;
};

typedef unsigned nfds_t;

#define POLLIN      0x0001
#define POLLPRI     0x0002
#define POLLOUT     0x0004
#define POLLERR     0x0008
#define POLLHUP     0x0010
#define POLLNVAL    0x0020

int poll(struct pollfd[], nfds_t, int);

__END_DECLS

#endif
