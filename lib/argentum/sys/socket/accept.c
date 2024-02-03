#include <sys/socket.h>
#include <sys/syscall.h>

int
accept(int socket, struct sockaddr *address, socklen_t * address_len)
{
  return __syscall(__SYS_ACCEPT, socket, (uint32_t) address,
                   (uint32_t) address_len, 0, 0, 0);
}
