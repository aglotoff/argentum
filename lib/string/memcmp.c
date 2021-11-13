#include <string.h>

/**
 * Compare bytes in memory.
 * 
 * @param s1 Pointer to the first object.
 * @param s2 Pointer to the second object.
 * @param n  The number of bytes to compare.
 * 
 * @return An integer greater than, equal to, or less than 0, if the object
 *         pointed to by s1 is greater than, equal to, or less than the object
 *         pointed to by s2, respectively. The sign of a non-zero value is
 *         determined by the sign of the difference between the values of the
 *         first byte that doesn't match in both memory blocks.
 */
int
memcmp(const void *s1, const void *s2, size_t n)
{
  const unsigned char *p1 = (const unsigned char *) s1;
  const unsigned char *p2 = (const unsigned char *) s2;

  while (n > 0 && *p1 == *p2) {
    p1++;
    p2++;
    n--;
  }

  return n == 0 ? 0 : (int) *p1 - *p2;
}
