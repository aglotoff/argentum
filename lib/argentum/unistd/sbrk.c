#include <sys/syscall.h>
#include <unistd.h>

void *
_sbrk(ptrdiff_t increment)
{
  return (void *) __syscall1(__SYS_SBRK, increment);
}
