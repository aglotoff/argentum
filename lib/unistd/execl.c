#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>

int
execl(const char *path, ...)
{
  char **argv;
  int argc;
  va_list ap;

  // Count the variable arguments.
  va_start(ap, path);
  for (argc = 0; va_arg(ap, char *) != NULL ; argc++)
    ;
  va_end(ap);

  if ((argv = (char **) malloc(sizeof(char *) * (argc + 1))) == NULL)
    return -1;

  // Copy the variable arguments into 'argv' array.
  va_start(ap, path);
  for (argc = 0; (argv[argc] = va_arg(ap, char *)) != NULL; argc++)
    ;
  argv[argc] = NULL;

  va_end(ap);

  return execv(path, argv);
}
