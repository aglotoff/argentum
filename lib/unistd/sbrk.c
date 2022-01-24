#include <syscall.h>
#include <unistd.h>

void *
sbrk(ptrdiff_t increment)
{
  return (void *) __syscall(__SYS_SBRK, increment, 0, 0);
}

