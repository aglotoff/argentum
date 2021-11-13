#include <string.h>

/**
 * String scanning operation.
 *
 * @param s C string.
 * @param c Character to be located.
 *
 * @return A pointer to the first occurence of c in s, or NULL if c is not 
 * found.
 */
char *
strchr(const char *s, int c)
{
  char ch = (char) c;
  while (*s) {
    if (*s == ch)
      return (char *) s;
    s++;
  }
  return NULL;
}
