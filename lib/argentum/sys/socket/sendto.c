#include <sys/socket.h>
#include <sys/syscall.h>

ssize_t
sendto(int socket, const void *message, size_t length, int flags,
       const struct sockaddr *dest_addr, socklen_t dest_len)
{
  return __syscall(__SYS_SENDTO, socket, (uint32_t) message, length, flags,
                   (uint32_t) dest_addr, dest_len);
}
