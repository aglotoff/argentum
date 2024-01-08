#include <fcntl.h>
#include <unistd.h>

int
dup(int fildes)
{
  return fcntl(fildes, F_DUPFD, 0);
}
