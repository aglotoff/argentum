#include <string.h>

void *
memset(void *s, int c, size_t n)
{
  unsigned char ch = (unsigned char) c;
  unsigned char *p = (unsigned char *) s;

  while (n--) {
    *p++ = ch;
  }

  return s;
}
