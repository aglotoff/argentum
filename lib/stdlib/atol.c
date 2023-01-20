#include <stdlib.h>

/**
 * Convert the initial portion of a string to long int representation.
 * 
 * @param nptr  Pointer to the string to be converted
 *
 * @return The converted value
 */
long
atol(const char *nptr)
{
  return strtol(nptr, NULL, 10);
}
