#include <stdio.h>

int
puts(const char *s)
{
  if (fputs(s, stdout) == EOF)
    return EOF;
  return fputc('\n', stdout);
}
