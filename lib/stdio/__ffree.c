#include <stdio.h>
#include <stdlib.h>

void
__ffree(FILE *stream)
{
  if (stream->mode & _MODE_ALLOC_FILE) {
    int i;

    for (i = 0; i < FOPEN_MAX; i++) {
      if (__files[i] == stream) {
        __files[i] = NULL;
        break;
      }
    }

    free(stream);
  }
}
