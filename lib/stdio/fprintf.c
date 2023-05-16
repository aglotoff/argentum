#include <stdio.h>

int
fprintf(FILE *stream, const char *format, ...)
{
  va_list ap;
  int ret;

  va_start(ap, format);
  ret = vfprintf(stream, format, ap);
  va_end(ap);

  return ret;
}
