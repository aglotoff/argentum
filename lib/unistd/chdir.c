#include <syscall.h>
#include <unistd.h>

/**
 * Change current working directory.
 * 
 * @param path Directory pathname.
 * 
 * @return 0 on success, -1 on error.
 */
int
chdir(const char *path)
{
  return __syscall(__SYS_CHDIR, (uint32_t) path, 0, 0);
}
