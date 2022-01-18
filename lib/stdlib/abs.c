#include <stdlib.h>

/**
 * Compute an integer absolute value.
 *
 * @param  j The integer value.
 * @return The absolute value of j.
 */
int
abs(int j)
{
  return (j < 0) ? -j : j;
}
