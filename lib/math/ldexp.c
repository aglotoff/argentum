#include <errno.h>
#include <float.h>
#include <math.h>

//
// Code adapted from "The Standard C Library" by P.J. Plauger.
//

/**
 * Compute the quantity 'x * 2**exp'.
 * 
 * @param x   The significand.
 * @param exp The exponent.
 * 
 * @return 'x' multiplied by 2 raised to the power 'exp'.
 */
double
ldexp(double x, int exp)
{
  int t;
  
  switch (__math_classify_double(&x)) {
  case FP_NAN:
    errno = EDOM;
    break;
  case FP_INFINITE:
    errno = ERANGE;
    break;
  case FP_ZERO:
    break;
  default:
    if (((t = __math_scale_double(&x, exp)) == FP_NAN) || (t == FP_INFINITE))
      errno = ERANGE;
    break;
  }

  return x;
}
