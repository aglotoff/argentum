#include <fcntl.h>
#include <unistd.h>

int
dup2(int fildes, int fildes2)
{
  close(fildes2);
  return fcntl(fildes, F_DUPFD, fildes2);
}
