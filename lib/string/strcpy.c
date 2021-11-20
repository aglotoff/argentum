#include <string.h>

/**
 * @brief Copy a string.
 * 
 * Copy the C string s2 (including the terminating null-character) to the 
 * array s1.
 * 
 * @param s1 Pointer to the array where the content is to be copied.
 * @param s2 C string to be copied.
 *
 * @return s1
 * 
 * @sa memcpy, memmove, strncpy
 */
char *
strcpy(char *s1, const char *s2)
{
  char *p;
  
  // Copy all characters from s2.
  for (p = s1; *s2 != '\0'; p++, s2++)
    *p = *s2;

  // Append the null-character.
  *p = '\0';

  return s1;
}
