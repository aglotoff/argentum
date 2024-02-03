#include <sys/syscall.h>
#include <unistd.h>

uid_t
getuid(void)
{
  return __syscall(__SYS_GETUID, 0, 0, 0, 0, 0, 0);
}
