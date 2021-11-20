#include <string.h>

/**
 * @brief Concatenate a string with part of another.
 * 
 * Appends at most n characters from the C string s2 (a null-character and
 * bytes that follow are not copied) to the end of the C string s1. A
 * terminating null-character is always appended to the result.
 * 
 * @param s1 Pointer to the destination array containing a C string.
 * @param s2 C string to be appended.
 * @param n  The maximum number of characters to copy.
 * 
 * @return s1.
 * 
 * @sa strcat
 */
char *
strncat(char *s1, const char *s2, size_t n)
{
  char *p;

  // Find the end of s1.
  for (p = s1; *p != '\0'; p++)
    ;

  // Copy at most n characters from s2.
  for ( ; (n != 0) && (*s2 != '\0'); n--)
    *p++ = *s2++;

  // Append the null-character.
  *p = '\0';

  return s1;
}
