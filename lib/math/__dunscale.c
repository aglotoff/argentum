#include <float.h>
#include <math.h>
#include <stdint.h>

int
__dunscale(double *x, int *exp_store)
{
	uint16_t *raw;
	int exp, frac;

	raw  = (uint16_t *) x;
	exp  = (raw[__D0] & __DBL_EXP) >> __DBL_EOFF;
	frac = (raw[__D0] & __DBL_FRAC) || raw[__D1] || raw[__D2] || raw[__D3];

	if (exp == __DBL_EMAX) {
		// NaN or Infinity
		*exp_store = 0;
		return frac ? FP_NAN : FP_INFINITE;
	}

	if (exp != 0 || ((exp = __dnormalize(raw)) != 0)) {
		// Normal value, simply update the exponent field.
		raw[__D0] = (raw[__D0] & ~__DBL_EXP) | ((__DBL_EBIAS - 1) << __DBL_EOFF);
		*exp_store = exp + 1 - __DBL_EBIAS;
		return FP_NORMAL;
	}

	// Zero.
	*exp_store = 0;
	return FP_ZERO;
}
