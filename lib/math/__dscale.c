#include <float.h>
#include <math.h>
#include <stdint.h>

/**
 * Scale the given value by a power of 2.
 *
 * @param x    The value to be scaled.
 * @param texp The power of 2 to which the number must be scaled.
 * 
 * @retval FP_ZERO      if the result is positive or negative zero
 * @retval FP_NORMAL    if the result is a normal value
 * @retval FP_SUBNORMAL if the result is a subnormal value
 * @retval FP_NAN       if the result is not-a-number
 * @retval FP_INFINITE  if the result is a positive or negative infinity
 */
int
__dscale(double *x, int texp)
{
  // ----------------------------------------------------------
  // Code adapted from "The Standard C Library" by P.J. Plauger
  // ----------------------------------------------------------

  uint16_t *raw;
  int exp, frac, sign;
  
  raw  = (uint16_t *) x;
  exp  = (raw[__D0] & __DBL_EXP) >> __DBL_EOFF;
  frac = (raw[__D0] & __DBL_FRAC) || raw[__D1] || raw[__D2] || raw[__D3];
  
  // If NaN or Infinity - do nothing.
  if (exp == __DBL_EMAX)
    return frac ? FP_NAN : FP_INFINITE;

  // If zero - do nothing.
  if (exp == 0 && (exp = __dnormalize(raw)) == 0)
    return FP_ZERO;

  // Adjust the exponent.
  exp += texp;

  // If the result is an overflow, return positive or negative infinity.
  if (exp >= __DBL_EMAX) {
    *x = raw[__D0] & __DBL_SIGN ?  -__dbl.inf.d : __dbl.inf.d;
    return FP_INFINITE;
  }

  // If the result is a normal value, simply update the exponent field.
  if (exp > 0) {
    raw[__D0] = (raw[__D0] & ~__DBL_EXP) | ((uint16_t) exp << __DBL_EOFF);
    return FP_NORMAL;
  }

  // Remember the sign bit.
  sign = raw[__D0] & __DBL_SIGN;
  raw[__D0] = (1 << __DBL_EOFF) | (raw[__D0] & __DBL_FRAC);

  // Check, whether the result could be represented as a subnormal value.
  if (exp >= -(__DBL_FBITS + 1)) {
    exp = -exp;

    // First, shift right by 16 bits at a time.
    for ( ; exp >= 16; exp -= 16) {
      raw[__D3] = raw[__D2];
      raw[__D2] = raw[__D1];
      raw[__D1] = raw[__D0];
      raw[__D0] = 0;
    }

    // Finish scaling.
    if (exp != 0) {
      raw[__D3] = (raw[__D3] >> exp) | (raw[__D2] << (16 - exp));
      raw[__D2] = (raw[__D2] >> exp) | (raw[__D1] << (16 - exp));
      raw[__D1] = (raw[__D1] >> exp) | (raw[__D0] << (16 - exp));
      raw[__D0] = (raw[__D0] >> exp);
    }

    // If no underflow - we have a subnormal value.
    if (raw[__D0] || raw[__D1] || raw[__D2] || raw[__D3]) {
      raw[__D0] |= sign;
      return FP_SUBNORMAL;
    }
  }

  // Guaranteed underflow - return zero.
  raw[__D0] = sign;
  raw[__D1] = raw[__D2] = raw[__D3] = 0;
  return FP_ZERO;
}
