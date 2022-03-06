// Test <math.h> functions
#include <assert.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdio.h>

#define E						2.71828182845904523536
#define LN2					0.69314718055994530942
#define SQRT_HALF   0.70710678118654752440    // sqrt(0.5)

static int
approx(double x, double y)
{
	return fabs((y != 0) ? (x - y) / y : x) < DBL_EPSILON;
}

int
main(void)
{
	double x;
	int e;
	
	// --------------------------------------------------------------------------
	// Exponential and logarithmic functions
	// --------------------------------------------------------------------------

	// exp
	assert(approx(exp(-1.0), 1.0 / E));
	assert(approx(exp(0.0), 1.0));
	assert(approx(exp(LN2), 2.0));
	assert(approx(exp(1.0), E));
	assert(approx(exp(3.0), E * E * E));

	// frexp
	assert(approx(frexp(-3.0, &e), -0.75) && (e == 2));
	assert(approx(frexp(-0.5, &e), -0.5) && (e == 0));
	assert((frexp(0.0, &e) == 0.0) && (e == 0));
	assert(approx(frexp(0.33, &e), 0.66) && (e == -1));
	assert(approx(frexp(0.66, &e), 0.66) && (e == 0));
	assert(approx(frexp(96.0, &e), 0.75) && (e == 7));

	// ldexp
	assert(ldexp(-3.0, 4) == -48.0);
	assert(ldexp(-0.5, 0) == -0.5);
	assert(ldexp(0.0, 36) == 0.0);
	assert(approx(ldexp(0.66, -1), 0.33));
	assert(ldexp(96, -3) == 12.0);

	// modf
	assert(approx(modf(-11.7, &x), -11.7 + 11.0) && (x == -11.0));
	assert((modf(-0.5, &x) == -0.5) && (x == 0.0));
	assert((modf(0.0, &x) == -0.0) && (x == 0.0));
	assert((modf(0.6, &x) == 0.6) && (x == 0.0));
	assert((modf(12.0, &x) == 0.0) && (x == 12.0));

	// log
	assert(log(1.0) == 0);
	assert(approx(log(E), 1.0));
	assert(approx(log(E * E * E), 3.0));

	// log10
	assert(approx(log10(10.0), 1.0));
	assert(approx(log10(5.0), 1.0 - log10(2.0)));
	assert(approx(log10(1e5), 5.0));

	// --------------------------------------------------------------------------
	// Power and absolute-value functions
	// --------------------------------------------------------------------------

	// fabs
  assert(fabs(-5.0) == 5.0);
	assert(fabs(0.0) == 0.0);
	assert(fabs(5.0) == 5.0);

	// sqrt
	assert(approx(sqrt(0.0), 0.0));
	assert(approx(sqrt(0.5), SQRT_HALF));
	assert(approx(sqrt(1.0), 1.0));
	assert(approx(sqrt(2.0), 1.0 / SQRT_HALF));
	assert(approx(sqrt(144.0), 12.0));

  // --------------------------------------------------------------------------
  // Nearest integer functions
  // --------------------------------------------------------------------------

	// ceil
	assert(ceil(-5.1) == -5.0);
	assert(ceil(-5.0) == -5.0);
	assert(ceil(-4.9) == -4.0);
	assert(ceil(0.0) == 0.0);
	assert(ceil(4.9) == 5.0);
	assert(ceil(5.0) == 5.0);
	assert(ceil(5.1) == 6.0);

	// floor
  assert(floor(-5.1) == -6.0);
	assert(floor(-5.0) == -5.0);
	assert(floor(-4.9) == -5.0);
	assert(floor(0.0) == 0.0);
	assert(floor(4.9) == 4.0);
	assert(floor(5.0) == 5.0);
	assert(floor(5.1) == 5.0);

  // --------------------------------------------------------------------------
  // Remainder functions
  // --------------------------------------------------------------------------

	// fmod
	assert(fmod(-7.0, 3.0) == -1.0);
	assert(fmod(-3.0, 3.0) == 0.0);
	assert(fmod(-2.0, 3.0) == -2.0);
	assert(fmod(0.0, 3.0) == 0.0);
	assert(fmod(2.0, 3.0) == 2.0);
	assert(fmod(3.0, 3.0) == 0.0);
	assert(fmod(7.0, 3.0) == 1.0);

  printf("SUCCESS testing <math.h>\n");
  return 0;
}
