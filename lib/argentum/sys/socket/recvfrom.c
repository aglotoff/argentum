#include <sys/socket.h>
#include <sys/syscall.h>

ssize_t
recvfrom(int socket, void *buffer, size_t length, int flags,
         struct sockaddr *address, socklen_t *address_len)
{
  return __syscall(__SYS_RECVFROM, socket, (uint32_t) buffer, length, flags,
                   (uint32_t) address, (uint32_t) address_len);
}
