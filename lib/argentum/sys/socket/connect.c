#include <sys/socket.h>
#include <sys/syscall.h>

int
connect(int socket, const struct sockaddr *address, socklen_t address_len)
{
  return __syscall(__SYS_CONNECT, socket, (uint32_t) address,
                   (uint32_t) address_len, 0, 0, 0);
}
