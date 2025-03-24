#include <sys/syscall.h>
#include <unistd.h>

int
lchown(const char *path, uid_t owner, gid_t group)
{
  // FIXME: symlinks
  return __syscall3(__SYS_CHOWN, path, owner, group);
}
