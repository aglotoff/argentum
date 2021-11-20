#include <string.h>

/**
 * @brief Scan string for a byte.
 * 
 * Find the first occurence in the C string s1 of any character from the C
 * string s2.
 * 
 * @param s1 The C string to be scanned.
 * @param s2 The C string containing the characters to match.
 * 
 * @return Pointer to the character or NULL if none of the characters in s2
 *         occurs in s1.
 * 
 * @sa memchr, strchr, strcspn, strrchr, strspn, strstr, strtok
 */
char *
strpbrk(const char *s1, const char *s2)
{
  for ( ; *s1 != '\0'; s1++)
    // Check whether the current character is part of s2.
    if (strchr(s2, *s1) != NULL)
      return (char *) s1;

  return NULL;
}
