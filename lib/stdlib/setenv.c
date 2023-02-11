#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
setenv(const char *envname, const char *envval, int overwrite)
{
  size_t name_len, val_len, environ_len;
  char **e;

  name_len = strlen(envname);
  val_len = strlen(envval);
  environ_len = 0;

  for (e = environ; *e != NULL; e++) {
    if ((strncmp(*e, envname, name_len) == 0) && ((*e)[name_len] == '='))
      break;
    environ_len += sizeof e;
  }

  if (*e != NULL && !overwrite)
    return 0;
  
  if (*e == NULL) {
    if ((environ = realloc(environ, environ_len + sizeof e)) == NULL)
      return -1;
    
    e = (char **) ((char *) environ + environ_len);
    e[1] = NULL;
  }
  
  if ((*e = malloc(name_len + val_len + 2)) == NULL)
    return -1;
  
  strncpy(*e, envname, name_len);
  (*e)[name_len] = '=';
  strcpy(*e + name_len + 1, envval);

  return 0;
}
