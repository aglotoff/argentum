#include <string.h>

/**
 * @brief Copy bytes in memory with overlapping areas.
 * 
 * Copy n bytes from the object pointed to by s2 into the object pointed to by
 * s1. Copying takes place as if a temporary buffer were used, allowing the
 * objects pointed to by s1 and s2 to overlap.
 * 
 * @param s1 Pointer to the block of memory to copy to.
 * @param s2 Pointer to the block of memoty to copy from.
 * @param n  The number of bytes to copy.
 * 
 * @return s1
 * 
 * @sa memcpy, strcpy, strncpy
 */
void *
memmove(void *s1, const void *s2, size_t n)
{
  char *dst = (char *) s1;
  const char *src = (const char *) s2;

  // Avoid using a temporary buffer using the following trick. If there is
  // an overlap that would prevent the correct operation of an ascending copy,
  // simply copy bytes backwards from the end of the source.
  if ((src < dst) && (src + n > dst)) {
    src += n;
    dst += n;
    for ( ; n > 0; n--)
      *--dst = *--src;
  } else {
    for ( ; n > 0; n--)
      *dst++ = *src++;
  }

  return s1;
}
