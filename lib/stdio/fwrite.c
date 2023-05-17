#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

size_t
fwrite(const void *ptr, size_t size, size_t nitems, FILE *stream)
{
  // TODO: implement

  


  return write(stream->fd, ptr, size * nitems);
}
