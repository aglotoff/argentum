#ifndef __MATH_H__
#define __MATH_H__

#include <stdint.h>

/** The value is positive or negative zero */
#define FP_ZERO       0
/** The value is normal */
#define FP_NORMAL	    1
/** The value is subnormal */
#define FP_SUBNORMAL	2
/** The value is not-a-number (NaN) */
#define FP_NAN		    3
/** The value is a positive or negative infinity */
#define FP_INFINITE	  4

#ifdef __cplusplus
extern "C" {
#endif

int    __math_classify_double(double *);
int    __math_normalize_double(uint16_t *);
int    __math_scale_double(double *, int);
int    __math_trunc_double(double *, int);
int    __math_unscale_double(double *, int *);
double __math_log(double, int);

double ceil(double);
double exp(double);
double fabs(double);
double floor(double);
double fmod(double, double);
double frexp(double, int *);
double ldexp(double, int);
double log(double);
double log10(double);
double modf(double, double *);
double sqrt(double);

#ifdef __cplusplus
};
#endif

#endif  // !__MATH_H__
