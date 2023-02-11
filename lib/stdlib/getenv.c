#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *
getenv(const char *name)
{
  size_t name_len = strlen(name);
  char **e;

  for (e = environ; *e != NULL; e++)
    if ((strncmp(*e, name, name_len) == 0) && ((*e)[name_len] == '='))
      return &(*e)[name_len + 1];

  return NULL;
}
