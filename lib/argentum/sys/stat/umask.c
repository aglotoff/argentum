#include <sys/stat.h>
#include <sys/syscall.h>

mode_t
umask(mode_t cmask)
{
  return __syscall1(__SYS_UMASK, cmask);
}
