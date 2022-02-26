#ifndef __MATH_H__
#define __MATH_H__

#include <stdint.h>

#define FP_ZERO       0
#define FP_NORMAL	    1
#define FP_SUBNORMAL	2
#define FP_NAN		    3
#define FP_INFINITE	  4

#ifdef __cplusplus
extern "C" {
#endif

int    __dclassify(double *);
int    __dnormalize(uint16_t *);
int    __dscale(double *, int);
int    __dtrunc(double *, int);
int    __dunscale(double *, int *);
double ceil(double);
double fabs(double);
double floor(double);
double fmod(double, double);
double frexp(double, int *);
double ldexp(double, int);
double modf(double, double *);

#ifdef __cplusplus
};
#endif

#endif  // !__MATH_H__
