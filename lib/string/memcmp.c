#include <string.h>

/**
 * @brief Compare bytes in memory.
 * 
 * Compares the first n bytes (each interpreted as unsigned char) of the object
 * pointed to by s1 to the first n bytes of the object pointed to by s2.
 * 
 * @param s1 Pointer to block of memory.
 * @param s2 Pointer to block of memory.
 * @param n  The number of bytes to compare.
 * 
 * @return An integer greater than, equal to, or less than 0, if the object
 *         pointed to by s1 is greater than, equal to, or less than the object
 *         pointed to by s2, respectively. The sign of a non-zero value is
 *         determined by the sign of the difference between the values of the
 *         first byte that doesn't match in both memory blocks.
 * 
 * @sa strcmp, strcoll, strncmp, strxfrm
 */
int
memcmp(const void *s1, const void *s2, size_t n)
{
  const unsigned char *p1 = (const unsigned char *) s1;
  const unsigned char *p2 = (const unsigned char *) s2;

  for ( ; n > 0; n--) {
    if (*p1 != *p2)
      return (int) *p1 - *p2;
    p1++;
    p2++;
  }

  return 0;
}
