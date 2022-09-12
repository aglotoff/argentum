#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

FILE *
fopen(const char *pathname, const char *mode)
{
  int i;

  for (i = 0; i < FOPEN_MAX; i++) {
    FILE *fp;

    if (__files[i] == NULL) {
      // Allocate a new file
      if ((fp = (FILE *) malloc(sizeof(FILE))) == NULL)
        return NULL;

      fp->mode   = _MODE_ALLOC_FILE;
      __files[i] = fp;
    } else if (__files[i]->mode == 0) {
      // Use a preallocated file
      fp = __files[i];
    } else {
      continue;
    }

    if (__fopen(fp, pathname, mode) != 0) {
      __ffree(fp);
      return NULL;
    }

    return fp;
  }

  errno = EMFILE;

  return NULL;
}
