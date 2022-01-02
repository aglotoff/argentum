#include <ctype.h>

/**
 * Test for an alphabetic character.
 * 
 * Check whether c is a character of class alpha in the current locale.
 * 
 * @param c The value to be checked, representable as an unsigned char or equal
 *          to the value of the macro EOF.
 *
 * @return Non-zero if c is an alphabetic character; 0 otherwise.
 * 
 * @sa isalnum, iscntrl, isdigit, isgraph, islower, isprint, ispunct, isspace,
 *     isupper, isxdigit
 */
int
(isalpha)(int c)
{
  return __ctest(c, __CALPHA);
}
