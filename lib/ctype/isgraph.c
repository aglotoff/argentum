#include <ctype.h>

/**
 * Test for an visible character.
 * 
 * Check whether c is a character of class graph in the current locale.
 * 
 * @param c The value to be checked, representable as an unsigned char or equal
 *          to the value of the macro EOF.
 *
 * @return Non-zero if c is an visible character; 0 otherwise.
 */
int
(isgraph)(int c)
{
  return __ctest(c, __CGRAPH);
}
