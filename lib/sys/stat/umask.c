#include <syscall.h>
#include <sys/stat.h>

mode_t
umask(mode_t cmask)
{
  return __syscall(__SYS_UMASK, cmask, 0, 0);
}
