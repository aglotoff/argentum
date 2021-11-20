#include <string.h>

/**
 * @brief Find a substring.
 * 
 * Locates the first occurrence of the string s2 in the string s1 (excluding
 * the terminating null character). If s2 points to a string with zero length,
 * returns s1.
 *
 * @param s1 The C string to be scanned.
 * @param s2 The C string containing the sequence of characters to match.
 * 
 * @return A pointer to the located string or NULL if the substring is not
 *         found.
 * 
 * @sa memchr, strchr, strcspn, strpbrk, strrchr, strspn, strtok
 */
char *
strstr(const char *s1, const char *s2)
{
  const char *p1, *p2;
  
  if (*s2 == '\0')
    return (char *) s1;
  
  // On each iteration, find the next occurrence of the first character of s2.
  for ( ; (s1 = strchr(s1, *s2)) != NULL; s1++) {
    // Compare the rest of the prefix.
    for (p1 = s1, p2 = s2; ; p1++, p2++) {
      if (*p2 == '\0')
        return (char *) s1;
      if (*p1 != *p2)
        break;
    }
  }

  return NULL;
}
