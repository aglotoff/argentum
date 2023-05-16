#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>

int
closedir(DIR *dirp)
{
  (void) dirp;

  fprintf(stderr, "TODO: closedir");
  abort();

  return -1;
}
