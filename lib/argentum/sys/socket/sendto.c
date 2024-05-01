#include <sys/socket.h>
#include <sys/syscall.h>

ssize_t
sendto(int socket, const void *message, size_t length, int flags,
       const struct sockaddr *dest_addr, socklen_t dest_len)
{
  return __syscall6(__SYS_SENDTO, socket, message, length, flags, dest_addr,
                    dest_len);
}
