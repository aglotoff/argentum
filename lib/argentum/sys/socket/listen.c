#include <sys/socket.h>
#include <sys/syscall.h>

int
listen(int socket, int backlog)
{
  return __syscall2(__SYS_LISTEN, socket, backlog);
}
