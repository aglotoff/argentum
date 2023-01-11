#include <errno.h>
#include <float.h>
#include <math.h>

//
// Code adapted from "The Standard C Library" by P.J. Plauger.
//

/**
 * Decompose the argument into integral and fraction parts, each of which has
 * the same sign as the argument.
 * 
 * @param x The value to be decomposed.
 * @param iptr Pointer to the memory location where to store the integral part.
 *
 * @return The fraction part of the argument.
 */
double
modf(double x, double *iptr)
{
  *iptr = x;

  switch (__math_trunc_double(iptr, 0)) {
  case FP_NAN:
    errno = EDOM;
    return x;
  case FP_INFINITE:
  case FP_ZERO:
    return 0.0;
  default:
    return x - *iptr;
  }
}
