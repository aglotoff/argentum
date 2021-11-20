#include <string.h>

/**
 * @brief String scanning operation.
 * 
 * Finds the last occurrence of c (converted to a char) in the C string s.
 * The terminating null-character is considered to be part of the string.
 *
 * @param s C string.
 * @param c The character to be located.
 *
 * @return A pointer to the character, or NULL if the character was not found.
 * 
 * @sa memchr, strchr, strcspn, strpbrk, strspn, strstr, strtok
 */
char *
strrchr(const char *s, int c)
{
  const char *last;
  char ch = (char) c;

  for (last = NULL; ; s++) {
    if (*s == ch)
      last = s;
    if (*s == '\0')
      break;
  }

  return (char *) last;
}
