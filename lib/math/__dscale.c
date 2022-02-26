#include <float.h>
#include <math.h>
#include <stdint.h>

int
__dscale(double *x, int texp)
{
	uint16_t *raw;
	int exp, frac, sign;
	
	raw  = (uint16_t *) x;
	exp  = (raw[__D0] & __DBL_EXP) >> __DBL_EOFF;
	frac = (raw[__D0] & __DBL_FRAC) || raw[__D1] || raw[__D2] || raw[__D3];
	
	if (exp == __DBL_EMAX)
    // NaN or Infinity, do nothing.
		return frac ? FP_NAN : FP_INFINITE;
	
	if (exp == 0 && (exp = __dnormalize(raw)) == 0)
    // Zero value, do nothing.
		return FP_ZERO;

	// Adjust the exponent.
	exp += texp;

	if (exp >= __DBL_EMAX) {
		// Overflow, return +/- infinity.
		*x = raw[__D0] & __DBL_SIGN ?  -__dbl.inf.d : __dbl.inf.d;
		return FP_INFINITE;
	}

	if (exp > 0) {
		// Normal value, simply update the exponent field.
		raw[__D0] = (raw[__D0] & ~__DBL_EXP) | ((uint16_t) exp << __DBL_EOFF);
		return FP_NORMAL;
	}

	// Remember the sign bit.
	sign = raw[__D0] & __DBL_SIGN;
	raw[__D0] = (1 << __DBL_EOFF) | (raw[__D0] & __DBL_FRAC);

	if (exp >= -(__DBL_FBITS + 1)) {
		// First, shift right by 16 bits at a time.
		for ( ; exp <= -16; exp += 16) {
			raw[__D3] = raw[__D2];
			raw[__D2] = raw[__D1];
			raw[__D1] = raw[__D0];
			raw[__D0] = 0;
		}

		// Then, scale by 1 bit at a time.
		if ((exp = -exp) != 0) {
			raw[__D3] = (raw[__D3] >> exp) | (raw[__D2] << (16 - exp));
			raw[__D2] = (raw[__D2] >> exp) | (raw[__D1] << (16 - exp));
			raw[__D1] = (raw[__D1] >> exp) | (raw[__D0] << (16 - exp));
			raw[__D0] = (raw[__D0] >> exp);
		}

		// No underflow - return a subnormal value.
		if (raw[__D0] || raw[__D1] || raw[__D2] || raw[__D3]) {
			raw[__D0] |= sign;
			return FP_SUBNORMAL;
		}
	}

	// Underflow - return zero.
	raw[__D0] = sign;
	raw[__D1] = raw[__D2] = raw[__D3] = 0;
	return FP_ZERO;
}
