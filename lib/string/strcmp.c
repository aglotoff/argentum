#include <string.h>

/**
 * Compare two strings.
 *
 * @param str1 C string to be compared.
 * @param str2 C string to be compared.
 *
 * @return An integer less than, equal to, or greater than zero, if str1 is
 * less than, equal, or greater than str2, respectively.
 */
int
strcmp(const char *str1, const char *str2)
{
  while (*str1 && *str1 == *str2) {
    str1++;
    str2++;
  }
  return (int) *str1 - *str2;
}
