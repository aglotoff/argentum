#include <syscall.h>
#include <unistd.h>

/**
 * Change current working directory.
 * 
 * @param fildes File descriptor specifying the new current working directory.
 * 
 * @return 0 on success, -1 on error.
 */
int
fchdir(int fildes)
{
  return __syscall(__SYS_FCHDIR, fildes, 0, 0);
}
