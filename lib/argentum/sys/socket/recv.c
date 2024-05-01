#include <sys/socket.h>
#include <sys/syscall.h>

ssize_t
recv(int socket, void *buffer, size_t length, int flags)
{
  return __syscall4(__SYS_RECVFROM, socket, buffer, length, flags);
}
