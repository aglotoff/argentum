#include <stdio.h>
#include <sys/socket.h>
#include <sys/syscall.h>

int
bind(int socket, const struct sockaddr *address, socklen_t address_len)
{
  return __syscall(__SYS_BIND, socket, (uint32_t) address, (uint32_t) address_len);
}
