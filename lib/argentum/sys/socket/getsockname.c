#include <sys/socket.h>
#include <stdio.h>

int
getsockname(int socket, struct sockaddr *address, socklen_t *address_len)
{
  fprintf(stderr, "TODO: getsockname()\n");
  (void) socket;
  (void) address;
  (void) address_len;
  return -1;
}
