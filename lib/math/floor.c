#include <errno.h>
#include <math.h>

/**
 * Compute the largest integral value not greater than the argument.
 *
 * @param x A double value.
 * @return The largest integral value not greater than the argument.
 */
double
floor(double x)
{
  // ----------------------------------------------------------
  // Code adapted from "The Standard C Library" by P.J. Plauger
  // ----------------------------------------------------------

  switch (__dtrunc(&x, 0)) {
  case FP_NAN:
    errno = EDOM;
    return x;
  case FP_NORMAL:
  case FP_SUBNORMAL:
    return (x < 0.0) ? x - 1.0 : x;
  }

  return x;
}
