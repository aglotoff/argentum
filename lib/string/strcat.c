#include <string.h>

/**
 * @brief Concatenate two strings.
 * 
 * Appends a copy of the C string s2 (including the terminating null-character)
 * to the end of the C string s1.
 * 
 * @param s1 Pointer to the destination array containing a C string.
 * @param s2 C string to be appended.
 * 
 * @return s1.
 * 
 * @sa strncat
 */
char *
strcat(char *s1, const char *s2)
{
  char *p;

  // Find the end of s1.
  for (p = s1; *p != '\0'; p++)
    ;

  // Copy all characters from s2.
  for ( ; *s2 != '\0'; p++, s2++)
    *p = *s2;

  // Append the null-character.
  *p = '\0';

  return s1;
}
