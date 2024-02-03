#include <sys/syscall.h>
#include <unistd.h>

void *
_sbrk(ptrdiff_t increment)
{
  return (void *) __syscall(__SYS_SBRK, increment, 0, 0, 0, 0, 0);
}
