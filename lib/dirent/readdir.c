#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>

struct dirent *
readdir(DIR *dirp)
{
  (void) dirp;

  fprintf(stderr, "TODO: readdir");
  abort();

  return NULL;
}
