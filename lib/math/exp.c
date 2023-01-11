#include <errno.h>
#include <float.h>
#include <math.h>

// 1/ln(2)
#define LN2_INV 1.4426950408889634074

// Constants
#define C1  0.693359375
#define C2  -2.1219444005469058277E-4

// Coefficiens to compute R(g)
#define P0  0.249999999999999993E+0
#define P1  0.694360001511792852E-2
#define P2  0.165203300268279130E-4
#define Q0  0.500000000000000000E+0
#define Q1  0.555538666969001188E-1
#define Q2  0.495862884905441294E-3

double
exp(double x)
{
  double g, z, r;
  int neg, n;

  // TODO: check for too large or too small values

  neg = 0;
  if (x < 0) {
    x = -x;
    neg = 1;
  }

  // Check for special cases
  switch(__math_classify_double(&x)) {
  case FP_NAN:
    return x;
  case FP_INFINITE:
    errno = ERANGE;
    return __DSIGN(x) ? 0.0 : x;
  case FP_ZERO:
    return 1.0;
  }

  n = (int) (x * LN2_INV + 0.5);
  
  g = (x - n * C1) - n * C2;

  // R(g) = .5 + g * P(z) / (Q(z) - g * P(z)), where z = g^2
  z = g * g;
  r = ((P2 * z + P1) * z + P0) * g;
  r = 0.5 + r / (((Q2 * z + Q1) * z + Q0) - r);

  __math_scale_double(&r, ++n);

  return neg ? 1 / r : r;
}
