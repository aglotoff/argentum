#include <math.h>

/**
 * Compute natural logarithm.
 *
 * A domain error occurs if the argument is negative. A range error occurs if
 * the arguent is zero.
 *
 * @param x The value whose logarithm is calculated.
 * 
 * @return The natural logarithm of x.
 */
double
log(double x)
{
  return __math_log(x, 0);
}
