#include <stdio.h>

int
fputs(const char *s, FILE *stream)
{
  // TODO: manipulate the stream buffer directly instead of calling fputc?
  while (*s != '\0')
    if (fputc(*s++, stream) == EOF)
      return EOF;

  return 0;
}
