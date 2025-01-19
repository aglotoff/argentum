#include <stdio.h>
#include <sys/syscall.h>
\
int
_rename(const char *old, const char *new)
{
  return __syscall2(__SYS_RENAME, old, new);
}
