#include <math.h>

/**
 * Compute base-ten logarithm.
 * 
 * A domain error occurs if the argument is negative. A range error occurs if
 * the arguent is zero.
 *
 * @param x The value whose logarithm is calculated.
 * 
 * @return The base-ten logarithm of x.
 */
double
log10(double x)
{
  return __math_log(x, 1);
}
