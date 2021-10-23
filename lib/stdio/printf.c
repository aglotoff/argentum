#include <stdio.h>

int
printf(const char *format, ...)
{
  va_list ap;
  int ret;

  va_start(ap, format);
  ret = vprintf(format, ap);
  va_end(ap);

  return ret;
}
