#include <errno.h>
#include <float.h>
#include <math.h>

/**
 * Break the argument into a normalized fraction in the range [0.5, 1) and
 * an integral exponent.
 * 
 * @param x   A floating-point value.
 * @param exp Pointer to the memory location where to store the exponent.
 * 
 * @return The fraction value in the range [0.5, 1).
 */
double
frexp(double num, int *exp)
{
  // ----------------------------------------------------------
  // Code adapted from "The Standard C Library" by P.J. Plauger
  // ----------------------------------------------------------

  int bin_exp;

  switch (__dunscale(&num, &bin_exp)) {
  case FP_NAN:
  case FP_INFINITE:
    errno = EDOM;
    *exp = 0;
    return num;
  case FP_ZERO:
    *exp = 0;
    return 0.0;
  default:
    *exp = bin_exp;
    return num;
  }
}
