#include <sys/socket.h>
#include <sys/syscall.h>

ssize_t
recvfrom(int socket, void *buffer, size_t length, int flags,
         struct sockaddr *address, socklen_t *address_len)
{
  return __syscall6(__SYS_RECVFROM, socket, buffer, length, flags, address,
                    address_len);
}
