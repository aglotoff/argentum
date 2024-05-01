#include <stdio.h>
#include <sys/socket.h>
#include <sys/syscall.h>

int
bind(int socket, const struct sockaddr *address, socklen_t address_len)
{
  return __syscall3(__SYS_BIND, socket, address, address_len);
}
