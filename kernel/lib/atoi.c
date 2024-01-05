#include <stdlib.h>



/**
 * Convert the initial portion of a string to int representation.
 * 
 * @param nptr  Pointer to the string to be converted
 *
 * @return The converted value
 */
int
atoi(const char *nptr)
{
  return (int) strtol(nptr, NULL, 10);
}
