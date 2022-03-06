#include <errno.h>
#include <float.h>
#include <math.h>

// Polynomial coefficients
#define A0  -0.64124943423745581147E+2
#define A1   0.16383943563021534222E+2
#define A2  -0.78956112887491257267E+0
#define B0  -0.76949932108494879777E+3
#define B1   0.31203222091924532844E+3
#define B2  -0.35667977739034646171E+2

#define C0   0.70710678118654752440       // sqrt(0.5)
#define C1   0.693359375                  // 355.0 / 512.0
#define C2  -2.121944400546905827679E-4
#define C3   0.43429448190325182765       // log(e)

double
__log(double x, int dec)
{
  // --------------------------------------------------------------------------
  // Based on "Software Manual for the Elementary Functions" by Cody & Waite
  // --------------------------------------------------------------------------

  int n;
  double znum, zden, z, w, r, result;

  if (__DSIGN(x)) {
    errno = EDOM;
    return __dbl.nan.d;
  }

  switch (__dunscale(&x, &n)) {
  case FP_NAN:
    return x;
  case FP_INFINITE:
    return __dbl.inf.d;
  case FP_ZERO:
    errno = ERANGE;
    return -__dbl.inf.d;
  }

  if (x > C0) {
    znum = (x - 0.5) - 0.5;
    zden = x * 0.5 + 0.5;
  } else {
    n--;
    znum = x - 0.5;
    zden = znum * 0.5 + 0.5;
  }

  z = znum / zden;
  w = z * z;
  r = w * ((A2 * w + A1) * w + A0) / (((w + B2) * w + B1) * w + B0);
  r = z + z * r;

  result = (n * C2 + r) + n * C1;

  return dec ? (C3 * result) : result;
}
