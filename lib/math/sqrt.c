#include <errno.h>
#include <float.h>
#include <math.h>

//
// Code based on "Software Manual for the Elementary Functions" by Cody & Waite
//

#define SQRT_0_5    0.70710678118654752440    

/**
 * Compute square root.
 * 
 * A domain error occurs if the argument is negative.
 * 
 * @param x The value whose square root is returned.
 *
 * @return The square root of he argument.
 */
double
sqrt(double x)
{
  int n;
  double y;

  // Domain error if the argument is negative.
  if (x < 0) {
    errno = EDOM;
    return __dbl.nan.d;
  }

  // Extract the significand and the exponent, and check for special cases.
  switch (__math_unscale_double(&x, &n)) {
  case FP_NAN:
    errno = EDOM;
    return x;
  case FP_INFINITE:
    errno = ERANGE;
    return __dbl.inf.d;
  case FP_ZERO:
    return 0.0;
  }

  // Compute sqrt(f) using the Newton's method:
  // y(i) = (y(i-1) + f/y(i-1)) / 2,  i = 1,2,...,j
  //
  // y(0) (Hart et al, Computer Approximations, 1968).
  y = 0.41731 + 0.59016 * x;

  // For 64-bit double values, 3 iterations are sufficient:
  y += x / y;             // Save one multiply by skipping y(1)
  y = 0.25 * y + x / y;   // y(2)
  y = 0.5 * (y + x / y);  // y(3)

  // sqrt(x) = sqrt(f) * 2**(e/2),                if e is even
  // sqrt(x) = [sqrt(f)/sqrt(2)] * 2**[(e+1)/2],  if e is odd
  if ((n % 2) == 1) {
    // Multiplying by sqrt(0.5) is possibly faster than dividing by sqrt(2).
    y *= SQRT_0_5;
    n++;
  }

  __math_scale_double(&y, n / 2);

  return y;
}
