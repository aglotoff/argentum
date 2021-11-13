#include <string.h>

/**
 * Get length of fixed size string.
 *
 * @param s C string.
 *
 * @return The number of bytes preceding the null byte in the array to which s
 *         points.
 */
size_t
strlen(const char *s)
{
  const char *p;

  for (p = s; *p; p++)
    ;
  return (size_t) (p - s);
}
