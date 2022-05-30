#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * Remove a file.
 * 
 * @param path The pathname of the file to be removed.
 * @returns 0 on success, -1 otherwise.
 */
int
remove(const char *path)
{
  struct stat st;
  int r;

  if ((r = stat(path, &st)) < 0)
    return r;
  
  if (S_ISDIR(st.st_mode))
    return rmdir(path);

  return unlink(path);
}
