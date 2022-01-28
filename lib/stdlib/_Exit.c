#include <stdlib.h>
#include <syscall.h>

void
_Exit(int status)
{
  __syscall(__SYS_EXIT, status, 0, 0);
}
