#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
execvp(const char *path, char *const argv[])
{
  char *pathenv;
  size_t pathlen;

  if ((path[0] == '/') ||
      ((path[0] == '.') && ((path[1] == '/') ||
                            ((path[1] == '.') && (path[2] == '/')))))
    return execv(path, argv);

  if ((pathenv = getenv("PATH")) == NULL)
    return execv(path, argv);

  pathlen = strlen(path);
  
  while (*pathenv != '\0') {
    char *sep, *full;
    size_t len;
    
    if ((sep = strchr(pathenv, ':')) != NULL)
      len = sep - pathenv;
    else
      len = strlen(pathenv);
    
    if ((full = (char *) malloc(len + pathlen + 2)) == NULL)
      return -1;
    
    strncpy(full, pathenv, len);
    full[len] = '/';
    strncpy(&full[len + 1], path, pathlen + 1);

    execv(full, argv);

    free(full);

    if (sep == NULL)
      break;

    pathenv = sep + 1; 
  }

  return -1;
}
