#include <ctype.h>

/**
 * Test for a lowercase letter.
 * 
 * Check whether c is a character of class lower in the current locale.
 * 
 * @param c The value to be checked, representable as an unsigned char or equal
 *          to the value of the macro EOF.
 *
 * @return Non-zero if c is a lowercase letter; 0 otherwise.
 */
int
(islower)(int c)
{
  return __ctest(c, __CLOWER);
}
