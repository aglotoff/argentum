#include <string.h>

/**
 * @brief Copy fixed length string.
 * 
 * Copies up to n characters from the C string s2 to the array s1. Characters
 * that follow a null-character are not compared.
 * 
 * If the C string s2 is shorter than n characters, append null-characters to
 * the end of s1 until a total of n characters are written.
 * 
 * @param s1 Pointer to the array where the content is to be copied.
 * @param s2 C string to be copied.
 * @param n  The maximum number of characters to copy.
 * 
 * @return s1
 * 
 * @sa strncpy
 */
char *
strncpy(char *s1, const char *s2, size_t n)
{
  char *p = s1;
  
  // Copy at most n bytes from s2.
  for ( ; (n > 0) && (*s2 != '\0'); n--)
    *p++ = *s2++;

  // Append padding null-characters.
  for ( ; n > 0; n--)
    *p++ = '\0';

  return s1;
}
