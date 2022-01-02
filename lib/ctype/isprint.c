#include <ctype.h>

/**
 * Test for a printable character.
 * 
 * Check whether c is a character of class print in the current locale.
 * 
 * @param c The value to be checked, representable as an unsigned char or equal
 *          to the value of the macro EOF.
 *
 * @return Non-zero if c is a printable character; 0 otherwise.
 * 
 * @sa isalnum, isalpha, iscntrl, isdigit, isgraph, islower, ispunct, isspace,
 *     isupper, isxdigit
 */
int
(isprint)(int c)
{
  return __ctest(c, __CPRINT);
}
