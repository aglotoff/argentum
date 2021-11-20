#include <string.h>

/**
 * @brief Compare part of two strings.
 * 
 * Compares up to n characters from the C string s1 to the C string s2.
 * Characters that follow a null-character are not compared.
 * 
 * @param s1 C string to be compared.
 * @param s2 C string to be compared.
 * @param n  The maximum number of characters to compare.
 * 
 * @return An integer greater than, equal to, or less than 0, if the C string
 *         s1 is greater than, equal to, or less than the C string s2,
 *         respectively. The sign of a non-zero value is determined by the sign
 *         of the difference between the values of the first character (both
 *         interpreted as an unsigned char) that doesn't match in both strings.
 * 
 * @sa memcmp, strcmp, strcoll, strxfrm
 */
int
strncmp(const char *s1, const char *s2, size_t n)
{
  for ( ; n > 0; n--) {
    if (*s1 != *s2)
      return (int) (*(unsigned char *) s1) - (*(unsigned char *) s2);
    if (*s1 == '\0')
      break;
    s1++;
    s2++;
  }

  return 0;
}
