#include <ctype.h>

/**
 * Test for an uppercase letter.
 * 
 * Check whether c is a character of class upper in the current locale.
 * 
 * @param c The value to be checked, representable as an unsigned char or equal
 *          to the value of the macro EOF.
 *
 * @return Non-zero if c is an uppercase letter; 0 otherwise.
 */
int
(isupper)(int c)
{
  return __ctest(c, __CUPPER);
}
