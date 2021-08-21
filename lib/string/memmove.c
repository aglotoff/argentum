#include <string.h>

void *
memmove(void *s1, const void *s2, size_t n)
{
  char *dst = (char *) s1;
  const char *src = (const char *) s2;

  if ((src < dst) && (src + n > dst)) {
    src += n;
    dst += n;
    while (n-- > 0) {
      *--dst = *--src;
    }
  } else {
    while (n-- > 0) {
      *dst++ = *src++;
    }
  }

  return s1;
}
