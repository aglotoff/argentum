#include <ctype.h>

/**
 * Test for a punctuation character.
 * 
 * Check whether c is a character of class punct in the current locale.
 * 
 * @param c The value to be checked, representable as an unsigned char or equal
 *          to the value of the macro EOF.
 *
 * @return Non-zero if c is a punctuation character; 0 otherwise.
 * 
 * @sa isalnum, isalpha, iscntrl, isdigit, isgraph, islower, isprint, isspace,
 *     isupper, isxdigit
 */
int
(ispunct)(int c)
{
  return __ctest(c, __CPUNCT);
}
