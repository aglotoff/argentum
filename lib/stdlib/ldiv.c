#include <stdlib.h>

/**
 * Compute the quotient and remainder of a long integer division.
 *
 * @param  numer The numerator.
 * @param  denom The denominator.
 * @return A structure of type ldiv_t containing the quotient and remainder.
 */
ldiv_t
ldiv(long numer, long denom)
{
  ldiv_t result;

  result.quot = numer / denom;
  result.rem  = numer % denom;

  return result;
}
