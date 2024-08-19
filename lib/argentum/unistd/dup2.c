#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

int
dup2(int fildes, int fildes2)
{
  if (fildes == fildes2)
    return fildes2;

  close(fildes2);

  return fcntl(fildes, F_DUPFD, fildes2);
}
