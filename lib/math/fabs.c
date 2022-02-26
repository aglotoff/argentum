#include <errno.h>
#include <float.h>
#include <math.h>

double
fabs(double x)
{
	switch (__dclassify(&x)) {
	case FP_INFINITE:
		errno = ERANGE;
		return __dbl.inf.d;
	case FP_NAN:
		errno = EDOM;
		return x;
	case FP_ZERO:
		return 0.0;
	default:
		return (x < 0.0) ? -x : x;
	}
}
