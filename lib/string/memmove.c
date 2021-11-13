#include <string.h>

/**
 * Copy bytes in memory with overlapping areas.
 * 
 * @param s1 Pointer to the destination array where the data is to be copied.
 * @param s2 Pointer to the source of data to be copied.
 * @param n Number of bytes to copy.
 * 
 * @return s1
 */
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
