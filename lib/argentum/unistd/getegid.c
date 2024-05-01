#include <sys/syscall.h>
#include <unistd.h>

gid_t
getegid(void)
{
  return __syscall0(__SYS_GETEGID);
}
