#include <sys/socket.h>
#include <stdio.h>

int
getsockopt(int socket, int level, int option_name,
       void *option_value, socklen_t *option_len)
{
  fprintf(stderr, "TODO: getsockopt()\n");

  (void) socket;
  (void) option_name;
  (void) option_value;
  (void) option_len;

  return -1;
}
