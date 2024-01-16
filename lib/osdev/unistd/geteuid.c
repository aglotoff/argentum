#include <sys/syscall.h>
#include <unistd.h>

uid_t
geteuid(void)
{
  return __syscall(__SYS_GETEUID, 0, 0, 0, 0, 0, 0);
}
