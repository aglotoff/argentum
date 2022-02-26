#include <errno.h>
#include <float.h>
#include <math.h>

double
ldexp(double x, int exp)
{
	int t;
	
	switch (__dclassify(&x)) {
	case FP_NAN:
		errno = EDOM;
		break;
	case FP_INFINITE:
		errno = ERANGE;
		break;
	case FP_ZERO:
		break;
	default:
		if (((t = __dscale(&x, exp)) == FP_NAN) || (t == FP_INFINITE))
			errno = ERANGE;
		break;
	}

	return x;
}
