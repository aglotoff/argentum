#include <string.h>

/**
 * Locate the first occurence of a character in the C string.
 *
 * @param s The string
 * @param c The character to be located
 *
 * @return A pointer to the character, or NULL if the character was not found.
 */
char *
strchr(const char *s, int c)
{
  char ch = (char) c;

  for ( ; *s != ch; s++)
    if (*s == '\0')
      return NULL;

  return (char *) s;
}
