#include <float.h>
#include <math.h>
#include <stdint.h>

//
// Code adapted from "The Standard C Library" by P.J. Plauger.
//
// See the IEEE 754 standard to learn more about the representation of 
// floating-point values.
//

/**
 * Break the given value into a normalized fraction in the range [0.5, 1) and
 * a binary exponent.
 * 
 * @param x       Pointer to the value.
 * @param exp_ptr Pointer to the memory location where to store the exponent. 
 *
 * @retval FP_ZERO      if the result is positive or negative zero
 * @retval FP_NORMAL    if the result is a normal value
 * @retval FP_NAN       if the result is not-a-number
 * @retval FP_INFINITE  if the result is a positive or negative infinity
 */
int
__math_unscale_double(double *x, int *exp_ptr)
{
  uint16_t *raw;
  int exp, frac;

  raw = (uint16_t *) x;

  exp  = (raw[__D0] & __DBL_EXP) >> __DBL_EOFF;
  frac = (raw[__D0] & __DBL_FRAC) || raw[__D1] || raw[__D2] || raw[__D3];

  // If NaN or Infinity, do nothing.
  if (exp == __DBL_EMAX) {
    *exp_ptr = 0;
    return frac ? FP_NAN : FP_INFINITE;
  }

  // If we have a normalized value, simply update the exponent field.
  if ((exp != 0) || ((exp = __math_normalize_double(raw)) != 0)) {
    raw[__D0] = (raw[__D0] & ~__DBL_EXP) | ((__DBL_EBIAS - 1) << __DBL_EOFF);
    *exp_ptr = exp + 1 - __DBL_EBIAS;
    return FP_NORMAL;
  }

  // Zero.
  *exp_ptr = 0;
  return FP_ZERO;
}
