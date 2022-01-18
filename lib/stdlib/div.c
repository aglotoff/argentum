#include <stdlib.h>

/**
 * Compute the quotient and remainder of an integer division.
 *
 * @param  numer The numerator.
 * @param  denom The denominator.
 * @return A structure of type div_t containing the quotient and remainder.
 */
div_t
div(int numer, int denom)
{
  div_t result;

  result.quot = numer / denom;
  result.rem  = numer % denom;

  return result;
}
