#include <argentum/syscall.h>

void
_exit(int status)
{
  __syscall(__SYS_EXIT, status, 0, 0);
}
