#include <unistd.h>
#include <sys/syscall.h>

void
_exit(int status)
{
  __syscall1(__SYS_EXIT, status);
}
