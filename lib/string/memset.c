#include <string.h>

/**
 * @brief Set bytes in memory.
 *
 * Copies c (interpreted as an unsigned char) to the first n bytes of the
 * object pointed to by s.
 * 
 * @param s Pointer to the block of memory to fill.
 * @param c The value to be copied.
 * @param n The number of bytes to be set.
 * 
 * @return s.
 */
void *
memset(void *s, int c, size_t n)
{
  unsigned char *p = (unsigned char *) s;
  unsigned char uc = (unsigned char) c;

  for ( ; n > 0; n--)
    *p++ = uc;

  return s;
}
