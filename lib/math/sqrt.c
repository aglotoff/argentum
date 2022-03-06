#include <errno.h>
#include <float.h>
#include <math.h>

#define SQRT_HALF   0.70710678118654752440    // sqrt(0.5)

/**
 * Compute the square root of the argument.
 * 
 * @param x The value whose square root is returned.
 * @return The square root of he argument.
 */
double
sqrt(double x)
{
  // --------------------------------------------------------------------------
  // Based on "Software Manual for the Elementary Functions" by Cody & Waite
  // --------------------------------------------------------------------------
  //
  // Assume x > 0 and let x = f * 2**e, f in range [0.5, 1). Then:
  //   sqrt(x) = sqrt(f) * 2**(e/2),                if e is even
  //   sqrt(x) = [sqrt(f)/sqrt(2)] * 2**[(e+1)/2],  if e is odd
  //
  // To compute sqrt(x):
  // 1. Reduce x to the related parameters f and e
  // 2. Compute sqrt(f)
  // 3. Reconstruct sqrt(x) from the result
  //
  // Compute sqrt(f) using the Newton's method:
  //   y(i) = (y(i-1) + f/y(i-1)) / 2,  i = 1,2,...,j

  int n;
  double y;

  if (x < 0) {
    errno = EDOM;
    return __dbl.nan.d;
  }

  // Extract the significand and the exponent, and check for special cases.
  switch (__dunscale(&x, &n)) {
  case FP_NAN:
    errno = EDOM;
    return x;
  case FP_INFINITE:
    errno = ERANGE;
    return __dbl.inf.d;
  case FP_ZERO:
    return 0.0;
  }

  // y(0) (Hart et al, Computer Approximations, 1968).
  y = 0.41731 + 0.59016 * x;

  // For 64-bit double values, 3 iterations are sufficient:
  y += x / y;             // Save one multiply by skipping y(1)
  y = 0.25 * y + x / y;   // y(2)
  y = 0.5 * (y + x / y);  // y(3)

  // Multiplying by sqrt(0.5) is possibly faster than dividing by sqrt(2).
  if ((n % 2) == 1) {
    y *= SQRT_HALF;
    n++;
  }

  __dscale(&y, n / 2);

  return y;
}
