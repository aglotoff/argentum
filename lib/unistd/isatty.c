#include <fcntl.h>
#include <unistd.h>

int
isatty(int fildes)
{
  struct stat stat;

  if (fstat(fildes, &stat) != 0)
    return -1;

  // The only character device currently implemented is the console
  // TODO: use a more robust solution!

  return S_ISCHR(stat.st_mode);
}
