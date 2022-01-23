#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <unistd.h>

int
execl(const char *path, ...)
{
  char *argv[32];
  char *arg;
  int argc;
  va_list ap;

  argv[0] = (char *) path;
  argc = 1;

  va_start(ap, path);

  while ((arg = va_arg(ap, char *)) != NULL) {
    if (argc == 32) {
      // TODO: what should I return?
      errno = -E2BIG;
      return -1;
    }

    argv[argc++] = arg;
  }
  argv[argc] = NULL;

  return execvp(path, argv);
}
