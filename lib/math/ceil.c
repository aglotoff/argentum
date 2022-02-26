#include <math.h>

double
ceil(double x)
{
	int t;

  t = __dtrunc(&x, 0);

	if (((t == FP_NORMAL) || (t == FP_SUBNORMAL)) && (x > 0.0))
		return x + 1.0;
	return x;
}
