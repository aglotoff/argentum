#include <sys/syscall.h>
#include <unistd.h>

gid_t
getgid(void)
{
  return __syscall(__SYS_GETGID, 0, 0, 0, 0, 0, 0);
}
