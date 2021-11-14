#include <ctype.h>

/**
 * Test for an alphanumeric character.
 * 
 * Check whether c is a character of class alpha or digit in the current
 * locale.
 * 
 * @param c The value to be checked, representable as an unsigned char or equal
 *          to the value of the macro EOF.
 *
 * @return Non-zero if c is an alphanumeric character; 0 otherwise.
 */
int
(isalnum)(int c)
{
  return __ctest(c, __CALNUM);
}
