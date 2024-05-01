#include <sys/socket.h>
#include <sys/syscall.h>

int
socket(int domain, int type, int protocol)
{
  return __syscall3(__SYS_SOCKET, domain, type, protocol);
}
