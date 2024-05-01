#include <sys/syscall.h>
#include <unistd.h>

gid_t
getgid(void)
{
  return __syscall0(__SYS_GETGID);
}
