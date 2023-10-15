#include <string.h>

/**
 * Get length of a string.

 * @param s The C string
 * @return The length of s
 */
size_t
strlen(const char *s)
{
  const char *p;

  for (p = s; *p != '\0'; p++)
    ;

  return (size_t) (p - s);
}
