#include <string.h>

/**
 * @brief Get length of string.
 *
 * Computes the number of characters preceding the null character in the C
 * string s.
 *
 * @param s C string.
 *
 * @return The length of s.
 */
size_t
strlen(const char *s)
{
  const char *p;

  for (p = s; *p != '\0'; p++)
    ;

  return (size_t) (p - s);
}
