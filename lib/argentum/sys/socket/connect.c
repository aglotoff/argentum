#include <sys/socket.h>
#include <sys/syscall.h>

int
connect(int socket, const struct sockaddr *address, socklen_t address_len)
{
  return __syscall3(__SYS_CONNECT, socket, address, address_len);
}
