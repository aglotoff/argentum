#include <stdio.h>
#include <sys/select.h>
#include <poll.h>
#include <errno.h>

int
poll(struct pollfd fds[], nfds_t nfds, int timeout)
{
  fd_set read_fds, write_fds, except_fds;
  struct timeval tv;
  nfds_t i;
  int max_fd, r;

  FD_ZERO(&read_fds);
  FD_ZERO(&write_fds);
  FD_ZERO(&except_fds);

  max_fd = -1;
  for (i = 0; i < nfds; i++) {
    fds[i].revents = 0;

    if (fds[i].fd < 0)
      continue;

    if (fds[i].fd >= FD_SETSIZE) {
      errno = EINVAL;
      return -1;
    }

    if (fds[i].fd > max_fd)
      max_fd = fds[i].fd;

    if (fds[i].events & POLLIN)
      FD_SET(fds[i].fd, &read_fds);
    if (fds[i].events & POLLOUT)
      FD_SET(fds[i].fd, &write_fds);
    if (fds[i].events & POLLPRI)
      FD_SET(fds[i].fd, &except_fds);
  }

  tv.tv_sec = timeout / 1000;
  tv.tv_usec = (timeout % 1000) * 1000;

  r = select(max_fd + 1, &read_fds, &write_fds, &except_fds,
             timeout == -1 ? NULL : &tv);
  if (r <= 0)
    return r;

  r = 0;
  for (i = 0; i < nfds; i++) {
    if (fds[i].fd < 0)
      continue;

    if (FD_ISSET(fds[i].fd, &read_fds))
      fds[i].revents |= fds[i].events & POLLIN;
    if (FD_ISSET(fds[i].fd, &write_fds))
      fds[i].revents |= fds[i].events & POLLOUT;
    if (FD_ISSET(fds[i].fd, &except_fds))
      fds[i].revents |= fds[i].events & POLLPRI;

    if (fds[i].revents != 0)
      r++;
  }

  return r;
}
