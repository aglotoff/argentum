#include <sys/syscall.h>
#include <unistd.h>

int
ftruncate(int fildes, off_t length)
{
  return __syscall2(__SYS_FTRUNCATE, fildes, length);
}
