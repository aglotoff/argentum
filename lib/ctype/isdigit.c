#include <ctype.h>

/**
 * Test for a decimal digit.
 * 
 * Check whether c is a character of class digit in the current locale.
 * 
 * @param c The value to be checked, representable as an unsigned char or equal
 *          to the value of the macro EOF.
 *
 * @return Non-zero if c is a decimal digit; 0 otherwise.
 * 
 * @sa isalnum, isalpha, iscntrl, isgraph, islower, isprint, ispunct, isspace,
 *     isupper, isxdigit
 */
int
(isdigit)(int c)
{
  return __ctest(c, __CDIGIT);
}
