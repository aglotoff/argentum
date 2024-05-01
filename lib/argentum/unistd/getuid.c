#include <sys/syscall.h>
#include <unistd.h>

uid_t
getuid(void)
{
  return __syscall0(__SYS_GETUID);
}
