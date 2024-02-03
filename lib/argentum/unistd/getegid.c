#include <sys/syscall.h>
#include <unistd.h>

gid_t
getegid(void)
{
  return __syscall(__SYS_GETEGID, 0, 0, 0, 0, 0, 0);
}
