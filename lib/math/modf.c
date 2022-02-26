#include <float.h>
#include <math.h>

double
modf(double x, double *iptr)
{
	*iptr = x;

	switch (__dtrunc(iptr, 0)) {
	case FP_NAN:
		return x;
	case FP_INFINITE:
	case FP_ZERO:
		return 0.0;
	default:
		return x - *iptr;
	}
}
