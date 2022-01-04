#ifndef __INCLUDE_FLOAT_H__
#define __INCLUDE_FLOAT_H__

/**
 * @file include/float.h
 * 
 * Floating types.
 */

#include <yvals.h>

// log10(2)
#define __LG2             0.30102999566

/**
 * Radix of floating-point representation.
 */
#define FLT_RADIX         2

/**
 * The rounding mode for floating-point addition:
 * -1 - indeterminable,
 *  0 - toward zero,
 *  1 - to nearest,
 *  2 - toward positive infinity,
 *  3 - toward negative infinity.
 */
#define FLT_ROUNDS        __FLT_ROUNDS

/**
 * The number of base-FLT_RADIX mantissa digits, for the float data type.
 */
#define FLT_MANT_DIG      (__FLT_FBITS + 1)
/**
 * The precision in decimal digits for the float data type.
 */
#define FLT_DIG           ((int) (__FLT_FBITS * __LG2))
/**
 * Minimum negative integer such that FLT_RADIX raised to that power minus 1
 * can be represented as a normalized value of type double.
 */
#define FLT_MIN_EXP       (2 - __FLT_EBIAS)
/**
 * Minimum negative integer such that 10 raised to that power can be
 * represented as a normalized value of type float.
 */
#define FLT_MIN_10_EXP    ((int) ((1 - __FLT_EBIAS) * __LG2))
/**
 * Maximum integer such that FLT_RADIX raised to that power minus 1 can be
 * represented as a finite value of type float.
 */
#define FLT_MAX_EXP       (1 + __FLT_EBIAS)
/**
 * Maximum integer such that 10 raised to that power minus 1 can be represented
 * as a finite value of type float.
 */
#define FLT_MAX_10_EXP    ((int) ((1 + __FLT_EBIAS) * __LG2))

extern struct __flt {
  union {
    unsigned short w[2];
    float          f;
  } max, min, eps;
} __flt;

/**
 * Maximum representable finite floating-point number of type float.
 */
#define FLT_MAX           (__flt.max.f)
/**
 * The difference between 1 and the least value greater than 1 that is
 * representable in type float.
 */
#define FLT_EPSILON       (__flt.eps.f)
/**
 * Minimum normalized positive floating-point number of type float.
 */
#define FLT_MIN           (__flt.min.f)


/**
 * The number of base-FLT_RADIX mantissa digits, for the double data type.
 */
#define DBL_MANT_DIG      (__DBL_FBITS + 1)
/**
 * The precision in decimal digits for the double data type.
 */
#define DBL_DIG           ((int) (__DBL_FBITS * __LG2))
/**
 * Minimum negative integer such that FLT_RADIX raised to that power minus 1
 * can be represented as a normalized value of type double.
 */
#define DBL_MIN_EXP       (2 - __DBL_EBIAS)
/**
 * Minimum negative integer such that 10 raised to that power can be
 * represented as a normalized value of type double.
 */
#define DBL_MIN_10_EXP    ((int) ((1 - __DBL_EBIAS) * __LG2))
/**
 * Maximum integer such that FLT_RADIX raised to that power minus 1 can be
 * represented as a finite value of type double.
 */
#define DBL_MAX_EXP       (1 + __DBL_EBIAS)
/**
 * Maximum integer such that 10 raised to that power minus 1 can be represented
 * as a finite value of type double.
 */
#define DBL_MAX_10_EXP    ((int) ((1 + __DBL_EBIAS) * __LG2))

extern struct __dbl {
  union {
    unsigned short w[4];
    double         d;
  } max, min, eps;
} __dbl;

/**
 * Maximum representable finite floating-point number of type double.
 */
#define DBL_MAX           (__dbl.max.d)
/**
 * The difference between 1 and the least value greater than 1 that is
 * representable in type double.
 */
#define DBL_EPSILON       (__dbl.eps.d)
/**
 * Minimum normalized positive floating-point number of type double.
 */
#define DBL_MIN           (__dbl.min.d)


#if __LDBL

/**
 * The number of base-FLT_RADIX mantissa digits, for the long double data type.
 */
#define LDBL_MANT_DIG     (__LDBL_FBITS + 1)
/**
 * The precision in decimal digits for the long double data type.
 */
#define LDBL_DIG          ((int) (__LDBL_FBITS * __LG2))
/**
 * Minimum negative integer such that FLT_RADIX raised to that power minus 1
 * can be represented as a normalized value of type long double.
 */
#define LDBL_MIN_EXP      (2 - __LDBL_EBIAS)
/**
 * Minimum negative integer such that 10 raised to that power can be
 * represented as a normalized value of type long double.
 */
#define LDBL_MIN_10_EXP   ((int) ((1 - __LDBL_EBIAS) * __LG2))
/**
 * Maximum integer such that FLT_RADIX raised to that power minus 1 can be
 * represented as a finite value of type long double.
 */
#define LDBL_MAX_EXP      (1 + __LDBL_EBIAS)
/**
 * Maximum integer such that 10 raised to that power minus 1 can be represented
 * as a finite value of type long double.
 */
#define LDBL_MAX_10_EXP   ((int) ((1 + __LDBL_EBIAS) * __LG2))

extern struct __ldbl {
  union {
    unsigned short w[5];
    long double    ld;
  } max, min, eps;
} __ldbl;

/**
 * Maximum representable finite floating-point number of type long double.
 */
#define LDBL_MAX          (__ldbl.max.ld)
/**
 * The difference between 1 and the least value greater than 1 that is
 * representable in type long double.
 */
#define LDBL_EPSILON      (__ldbl.eps.ld)
/**
 * Minimum normalized positive floating-point number of type long double.
 */
#define LDBL_MIN          (__ldbl.min.ld)

#else

#define LDBL_MANT_DIG     DBL_MANT_DIG
#define LDBL_DIG          DBL_DIG
#define LDBL_MIN_EXP      DBL_MIN_EXP
#define LDBL_MIN_10_EXP   DBL_MIN_10_EXP
#define LDBL_MAX_EXP      DBL_MAX_EXP
#define LDBL_MAX_10_EXP   DBL_MAX_10_EXP
#define LDBL_MAX          DBL_MAX
#define LDBL_EPSILON      DBL_EPSILON
#define LDBL_MIN          DBL_MIN

#endif  // !__LDBL

#endif  // !__INCLUDE_FLOAT_H__
