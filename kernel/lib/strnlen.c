#include <string.h>

size_t
strnlen(const char *s, size_t maxlen)
{
  const char *p;
  size_t n;

  for (n = 0, p = s; (*p != '\0') && (n < maxlen); p++, n++)
    ;

  return n;
}
