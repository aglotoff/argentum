#include <sys/socket.h>
#include <sys/syscall.h>

int
accept(int socket, struct sockaddr *address, socklen_t * address_len)
{
  return __syscall3(__SYS_ACCEPT, socket, address, address_len);
}
