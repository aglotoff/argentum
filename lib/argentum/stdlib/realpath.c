#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <stdio.h>

char *
realpath(const char *path, char *resolved_path)
{
  // fprintf(stderr, "TODO: realpath(%s)\n", path);
  *resolved_path = '\0';

  if (*path != '/') {
    getcwd(resolved_path, PATH_MAX);
    strcat(resolved_path, "/");
  }

  strcat(resolved_path, path);
  return resolved_path;
}
