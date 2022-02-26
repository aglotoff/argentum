#include <float.h>
#include <math.h>
#include <stdint.h>

int
__dclassify(double *x)
{
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
