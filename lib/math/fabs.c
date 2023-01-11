#include <errno.h>
#include <float.h>
#include <math.h>

//
// Code adapted from "The Standard C Library" by P.J. Plauger.
//

/**
 * Compute the absolute value of the argument.
 * 
 * @param x The value whose absolute value is returned.
 * @return The absolute value of the argument.
 */
double
fabs(double x)
{
  switch (__math_classify_double(&x)) {
  case FP_INFINITE:
    errno = ERANGE;
    return __dbl.inf.d;
  case FP_NAN:
    errno = EDOM;
    return x;
  case FP_ZERO:
    return 0.0;
  default:
    return (x < 0.0) ? -x : x;
  }
}
