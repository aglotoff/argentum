#include <string.h>

/**
 * Set bytes in memory.
 *
 * @param s Pointer to the block of memory to fill.
 * @param c Value to be copied (interpreted as unsigned char).
 * @param n Number of bytes to be set.
 * 
 * @return s.
 */
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
