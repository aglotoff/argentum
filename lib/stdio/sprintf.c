#include <stdio.h>

int
sprintf(char *s, const char *format, ...)
{
  va_list ap;
  int ret;

  va_start(ap, format);
  ret = vsprintf(s, format, ap);
  va_end(ap);

  return ret;
}
