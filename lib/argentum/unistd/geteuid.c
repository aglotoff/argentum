#include <sys/syscall.h>
#include <unistd.h>

uid_t
geteuid(void)
{
  return __syscall0(__SYS_GETEUID);
}
