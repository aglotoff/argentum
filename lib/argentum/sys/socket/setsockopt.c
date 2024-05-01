#include <sys/socket.h>
#include <sys/syscall.h>

int
setsockopt(int socket, int level, int option_name, const void *option_value,
           socklen_t option_len)
{
  return __syscall5(__SYS_SETSOCKOPT, socket, level, option_name, option_value,
                    option_len);
}
