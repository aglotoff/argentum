#ifndef _POLL_H
#define _POLL_H

struct pollfd {
  int    fd;
  short  events;
  short  revents;
};

typedef unsigned nfds_t;

int poll(struct pollfd[], nfds_t, int);

#endif
