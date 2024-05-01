#include <sys/socket.h>
#include <sys/syscall.h>

ssize_t
send(int socket, const void *buffer, size_t length, int flags)
{
  return __syscall4(__SYS_SENDTO, socket, buffer, length, flags);
}
