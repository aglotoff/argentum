#include <float.h>
#include <math.h>
#include <stdint.h>

/**
 * Categorize the given floating-point value.
 * 
 * @param x Pointer to the value to be classified.
 * 
 * @retval FP_ZERO      if x is positive or negative zero
 * @retval FP_NORMAL    if x is a normal value
 * @retval FP_SUBNORMAL if x is a subnormal value
 * @retval FP_NAN       if x is not-a-number
 * @retval FP_INFINITE  if x is a positive or negative infinity
 */
int
__dclassify(double *x)
{
  // ----------------------------------------------------------
  // Code adapted from "The Standard C Library" by P.J. Plauger
  // ----------------------------------------------------------

  uint16_t *raw;
  int exp, frac;

  raw  = (uint16_t *) &x;
  exp  = (raw[__D0] & __DBL_EXP) >> __DBL_EOFF;
  frac = (raw[__D0] & __DBL_FRAC) || raw[__D1] || raw[__D2] || raw[__D3];

  if (exp == __FLT_EMAX)
    return frac ? FP_NAN : FP_INFINITE;
  if (exp == 0)
    return frac ? FP_SUBNORMAL : FP_ZERO;
  return FP_NORMAL;
}
