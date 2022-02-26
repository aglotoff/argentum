#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdint.h>

double
fmod(double x, double y)
{
	double t;
	int xtype, ytype, xexp, yexp, sign, n;

	xtype = __dclassify(&x);
	ytype = __dclassify(&y);

	if (xtype == FP_NAN) {
		errno = EDOM;
		return x;
	}
	if ((xtype == FP_ZERO) || (ytype == FP_INFINITE)) {
		return x;
	}
	if (ytype == FP_NAN) {
		errno = EDOM;
		return y;
	}
	if ((xtype == FP_INFINITE) || (ytype == FP_ZERO)) {
		errno = EDOM;
		return __dbl.nan.d;
	}
	
	if (x < 0.0) {
		x = -x;
		sign = 1;
	} else {
		sign = 0;
	}

	if (y < 0.0)
		y = -y;
	
	t = y;
	n = 0;
	__dunscale(&t, &yexp);

	// Repeatedly subtract |y| from |x| until the remainder is smaller than |y|.
	while (n >= 0) {
		// First, compare the exponents of |x| and |y| to determine whether or
		// not further subtraction might be possible.
		t = x;
		if ((__dunscale(&t, &xexp) == FP_ZERO) || ((n = xexp - yexp) < 0))
			break;

		// Try to subtract |y|*2^n.
		for ( ; n >= 0; n--) {
			t = y;
			__dscale(&t, n);
			if (t <= x) {
				x -= t;
				break;
			}
		}
	}

	return sign ? -x : x;
}
