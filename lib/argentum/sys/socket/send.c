#include <sys/socket.h>
#include <sys/syscall.h>

ssize_t
send(int socket, const void *buffer, size_t length, int flags)
{
  return __syscall(__SYS_SENDTO, socket, (uint32_t)buffer, length, flags, 0, 0);
}
