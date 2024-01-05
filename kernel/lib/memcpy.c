#include <string.h>

/**
 * @brief Copy bytes in memory.
 * 
 * Copy n bytes from the object pointed to by s2 into the object pointed to by
 * s1. The objects pointed to by s1 and s2 should not overlap.
 * 
 * @param s1 Pointer to the block of memory to copy to.
 * @param s2 Pointer to the block of memoty to copy from.
 * @param n  The number of bytes to copy.
 * 
 * @return s1
 * 
 * @sa memmove, strcpy, strncpy
 */
void *
memcpy(void *s1, const void *s2, size_t n)
{
  char *dst = (char *) s1;
  const char *src = (const char *) s2;

  for ( ; n > 0; n--)
    *dst++ = *src++;

  return s1;
}
