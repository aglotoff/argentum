#include <ctype.h>

/**
 * Test for a control character.
 * 
 * Check whether c is a character of class cntrl in the current locale.
 * 
 * @param c The value to be checked, representable as an unsigned char or equal
 *          to the value of the macro EOF.
 *
 * @return Non-zero if c is a control character; 0 otherwise.
 */
int
(iscntrl)(int c)
{
  return __ctest(c, __CCNTRL);
}
