#include <string.h>

/**
 * @brief Compare two strings.
 * 
 * Compares the C string s1 to the C string s2.
 *
 * @param str1 C string to be compared.
 * @param str2 C string to be compared.
 *
 * @return An integer greater than, equal to, or less than 0, if the C string
 *         s1 is greater than, equal to, or less than the C string s2,
 *         respectively. The sign of a non-zero value is determined by the sign
 *         of the difference between the values of the first character (both
 *         interpreted as an unsigned char) that doesn't match in both strings.
 * 
 * @sa memcmp, strcoll, strncmp, strxfrm
 */
int
strcmp(const char *s1, const char *s2)
{
  for ( ; *s1 == *s2; s1++, s2++)
    if (*s1 == '\0')
      return 0;

  return (int) (*(unsigned char *) s1) - (*(unsigned char *) s2);
}
