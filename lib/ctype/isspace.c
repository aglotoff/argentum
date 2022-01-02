#include <ctype.h>

/**
 * Test for a white-space character.
 * 
 * Check whether c is a character of class space in the current locale.
 * 
 * @param c The value to be checked, representable as an unsigned char or equal
 *          to the value of the macro EOF.
 *
 * @return Non-zero if c is a white-space character; 0 otherwise.
 * 
 * @sa isalnum, isalpha, iscntrl, isdigit, isgraph, islower, isprint, ispunct,
 *     isupper, isxdigit
 */
int
(isspace)(int c)
{
  return __ctest(c, __CWSPACE);
}
