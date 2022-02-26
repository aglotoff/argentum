#include <float.h>
#include <math.h>
#include <stdint.h>

/**
 * Truncate a double value by dropping all fraction bits less than the given
 * threshold. 
 * 
 * @param x    Pointer to the value to be truncated.
 * @param texp The power of 2 representing the truncation threshold. If zero,
 *             the value is rounded to the nearest integer.
 *
 * @retval FP_ZERO      if the result is positive or negative zero
 * @retval FP_NORMAL    if the result is a normal value
 * @retval FP_SUBNORMAL if the result is a subnormal value
 * @retval FP_NAN       if the result is not-a-number
 * @retval FP_INFINITE  if the result is a positive or negative infinity
 */
int
__dtrunc(double *x, int texp)
{
  // ----------------------------------------------------------
  // Code adapted from "The Standard C Library" by P.J. Plauger
  // ----------------------------------------------------------

  static int sub[] = { __D3, __D2, __D1, __D0 };

  uint16_t *raw;
  int exp, frac, drop_bits, drop_words;
  
  raw  = (uint16_t *) x;
  exp  = (raw[__D0] & __DBL_EXP) >> __DBL_EOFF;
  frac = (raw[__D0] & __DBL_FRAC) || raw[__D1] || raw[__D2] || raw[__D3];

  // If NaN or Infinity - do nothing.
  if (exp == __DBL_EMAX)
    return frac ? FP_NAN : FP_INFINITE;

  // If zero - do nothing.
  if ((exp == 0) && !frac)
    return FP_ZERO;

  // Determine the number of fraction bits to drop.
  drop_bits = __DBL_FBITS - (exp - __DBL_EBIAS) - texp;
  
  // If nothing to drop, return.
  if (drop_bits <= 0)
    return FP_ZERO;

  // Clear all fraction bits.
  if (drop_bits >= __DBL_FBITS) {
    raw[__D0] = raw[__D1] = raw[__D2] = raw[__D3] = 0;
    return exp ? FP_NORMAL : FP_SUBNORMAL;
  }

  drop_words = drop_bits >> 4;
  drop_bits &= 0xF;

  // Clear fraction bits in the highest word.
  frac = raw[sub[drop_words]] & ((1 << drop_bits) - 1);
  raw[sub[drop_words]] &= ~((1 << drop_bits) - 1);

  // Clear whole 16-bit words.
  while (drop_words) {
    frac |= raw[sub[--drop_words]];
    raw[sub[drop_words]] = 0;
  }

  if (!frac)
    return FP_ZERO;
  return exp ? FP_NORMAL : FP_SUBNORMAL;
}
