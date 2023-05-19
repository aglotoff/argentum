#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

void
setbuf(FILE *stream, char *buf)
{
  setvbuf(stream, buf, buf == NULL ? _IONBF : _IOFBF, BUFSIZ);
}
