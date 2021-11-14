#include <ctype.h>

/**
 * Transliterate uppercase characters to lowercase.
 * 
 * @param c The value to be converted, representable as an unsigned char or
 *          equal to the value of the macro EOF.
 * 
 * @return The lowercase letter corresponding to c, if such value exists; or c
 *         (unchanged) otherwise.
 */
int
(tolower)(int c)
{
  return c < 0 ? c : __tolower[(unsigned char) c];
}
