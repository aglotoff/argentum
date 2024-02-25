#include <stdio.h>
#include <sys/termios.h>
#include <unistd.h>

long
fpathconf(int fildes, int name)
{
  switch (name) {
  case _PC_VDISABLE:
    return (long) _POSIX_VDISABLE;
  default:
    fprintf(stderr, "TODO: fpathconf(%d,%d)\n", fildes, name);
    return -1;
  }
}
