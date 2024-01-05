#include <string.h>

/**
 * @brief String scanning operation.
 * 
 * Finds the first occurrence of c (converted to a char) in the C string s.
 * The terminating null-character is considered to be part of the string.
 *
 * @param s C string.
 * @param c The character to be located.
 *
 * @return A pointer to the character, or NULL if the character was not found.
 * 
 * @sa memchr, strcspn, strpbrk, strrchr, strspn, strstr, strtok
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
