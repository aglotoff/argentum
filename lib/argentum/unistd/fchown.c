#include <sys/syscall.h>
#include <unistd.h>

int
fchown(int fildes, uid_t owner, gid_t group)
{
  return __syscall3(__SYS_FCHOWN, fildes, owner, group);
}
