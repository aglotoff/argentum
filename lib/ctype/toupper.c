#include <ctype.h>

/**
 * Transliterate lowercase characters to uppercase.
 * 
 * @param c The value to be converted, representable as an unsigned char or
 *          equal to the value of the macro EOF.
 * 
 * @return The uppercase letter corresponding to c, if such value exists; or c
 *         (unchanged) otherwise.
 */
int
(toupper)(int c)
{
  return c < 0 ? c : __toupper[(unsigned char) c];
}
