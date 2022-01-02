#include <ctype.h>

/**
 * Test for a hexadecimal digit.
 * 
 * Check whether c is a character of class xdigit in the current locale.
 * 
 * @param c The value to be checked, representable as an unsigned char or equal
 *          to the value of the macro EOF.
 *
 * @return Non-zero if c is a hexadecimal digit; 0 otherwise.
 * 
 * @sa isalnum, isalpha, iscntrl, isdigit, isgraph, islower, isprint, ispunct,
 *     isspace, isupper
 */
int
(isxdigit)(int c)
{
  return __ctest(c, __CXDIGIT);
}
