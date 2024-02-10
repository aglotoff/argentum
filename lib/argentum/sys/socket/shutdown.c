#include <sys/socket.h>
#include <stdio.h>

int
shutdown(int socket, int how)
{
  fprintf(stderr, "TODO: shutdown()\n");
  (void) socket;
  (void) how;
  return -1;
}
