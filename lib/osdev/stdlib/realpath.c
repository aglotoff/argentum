#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

char *
realpath(const char *path, char *resolved_path)
{
  getcwd(resolved_path, PATH_MAX);
  strcat(resolved_path, "/");
  strcat(resolved_path, path);
  return resolved_path;
}
