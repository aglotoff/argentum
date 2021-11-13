#include <stdio.h>

int
snprintf(char *s, size_t n, const char *format, ...)
{
  va_list ap;
  int ret;

  va_start(ap, format);
  ret = vsnprintf(s, n, format, ap);
  va_end(ap);

  return ret;
}
