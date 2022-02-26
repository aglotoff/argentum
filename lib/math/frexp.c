#include <errno.h>
#include <float.h>
#include <math.h>

double
frexp(double num, int *exp)
{
	int bin_exp;

	switch (__dunscale(&num, &bin_exp)) {
	case FP_NAN:
	case FP_INFINITE:
		errno = EDOM;
		*exp = 0;
		return num;
	case FP_ZERO:
		*exp = 0;
		return 0.0;
	default:
		*exp = bin_exp;
		return num;
	}
}
