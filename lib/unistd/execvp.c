#include <string.h>
#include <unistd.h>

// TODO: malloc
static char full[256];

int
execvp(const char *path, char *const argv[])
{
  if ((path[0] == '/') ||
      (strncmp(path, "./", 2) == 0) ||
      (strncmp(path, "../", 3) == 0)) {
    execv(path, argv);
  } else {
    strncpy(full, "/bin/", sizeof(full) - 1);
    strncat(full, path, sizeof(full) - 1);
    execv(full, argv);
  }
  return 0;
}
