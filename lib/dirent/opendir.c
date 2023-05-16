#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>

DIR *
opendir(const char *dirname)
{
  (void) dirname;

  fprintf(stderr, "TODO: opendir");
  abort();

  return NULL;
}
