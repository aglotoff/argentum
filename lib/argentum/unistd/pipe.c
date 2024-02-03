#include <unistd.h>
#include <sys/syscall.h>

int
pipe(int fildes[2])
{
  return __syscall(__SYS_PIPE, (uintptr_t) fildes, 0, 0, 0, 0, 0);
}
