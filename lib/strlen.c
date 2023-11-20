#include <string.h>

/**
 * Compute the number of bytes in the string (not including the terminating
 * null byte).
 *
 * @param s The string
 * @return The length of the string
 */
size_t
strlen(const char *s)
{
  const char *p;

  for (p = s; *p != '\0'; p++)
    ;

  return (size_t) (p - s);
}
