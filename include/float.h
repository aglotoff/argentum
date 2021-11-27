#ifndef __INCLUDE_FLOAT_H__
#define __INCLUDE_FLOAT_H__

/**
 * @file include/float.h
 * 
 * Floating types.
 */

// Subscripts of words in floating-point representations.
// D0 is the most significant word.
#define __D0		      0
#define __D1		      (D0 + 1)
#define __D2		      (D1 + 1)
#define __D3		      (D2 + 1)

// Parameters of the float data type representation
#define __FLT_SIGN    (1 << 15)                       // Sign bit mask
#define __FLT_EOFF    7                               // Exponent offset in D0
#define __FLT_EBIAS   ((1 << (14 - __FLT_EOFF)) - 1)  // Exponent bias
#define __FLT_EMAX    ((1 << (15 - __FLT_EOFF)) - 1)  // Maximum exponent
#define __FLT_EXP     (__FLT_EMAX << __FLT_EOFF)      // Exponent mask
#define __FLT_FBITS   (16 + __FLT_EOFF)               // # of fraction bits
#define __FLT_FRAC    ((1 << __FLT_EOFF) - 1)         // Fraction mask

// Parameters of the double data type representation
#define __DBL_SIGN    (1 << 15)                       // Sign bit mask
#define __DBL_EOFF    4                               // Exponent offset in D0
#define __DBL_EBIAS   ((1 << (14 - __DBL_EOFF)) - 1)  // Exponent bias
#define __DBL_EMAX    ((1 << (15 - __DBL_EOFF)) - 1)  // Maximum exponent
#define __DBL_EXP     (__DBL_EMAX << __DBL_EOFF)      // Exponent mask
#define __DBL_FBITS   (48 + __DBL_EOFF)               // # of fraction bits
#define __DBL_FRAC    ((1 << __DBL_EOFF) - 1)         // Fraction mask

// Parameters of the long double data type representation
#define __LDBL_SIGN   (1 << 15)                       // Sign bit mask
#define __LDBL_EOFF   4                               // Exponent offset in D0
#define __LDBL_EBIAS  ((1 << (14 - __LDBL_EOFF)) - 1) // Exponent bias
#define __LDBL_EMAX   ((1 << (15 - __LDBL_EOFF)) - 1) // Maximum exponent
#define __LDBL_EXP    (__LDBL_EMAX << __LDBL_EOFF)    // Exponent mask
#define __LDBL_FBITS  (48 + __LDBL_EOFF)              // # of fraction bits
#define __LDBL_FRAC   ((1 << __LDBL_EOFF) - 1)        // Fraction mask

// log10(2)
#define __LG2         0.30102999566

/**
 * Radix of floating-point representation.
 */
#define FLT_RADIX         2

/**
 * The number of base-FLT_RADIX mantissa digits, for the float data type.
 */
#define FLT_MANT_DIG      (__FLT_FBITS + 1)
/**
 * The number of base-FLT_RADIX mantissa digits, for the double data type.
 */
#define DBL_MANT_DIG      (__DBL_FBITS + 1)
/**
 * The number of base-FLT_RADIX mantissa digits, for the long double data type.
 */
#define LDBL_MANT_DIG     (__LDBL_FBITS + 1)

/**
 * The precision in decimal digits for the float data type.
 */
#define FLT_DIG           ((int) (__FLT_FBITS * __LG2))
/**
 * The precision in decimal digits for the double data type.
 */
#define DBL_DIG           ((int) (__DBL_FBITS * __LG2))
/**
 * The precision in decimal digits for the long double data type.
 */
#define LDBL_DIG          ((int) (__LDBL_FBITS * __LG2))

/**
 * Minimum negative integer such that FLT_RADIX raised to that power minus 1
 * can be represented as a normalized value of type double.
 */
#define FLT_MIN_EXP       (2 - __FLT_EBIAS)
/**
 * Minimum negative integer such that FLT_RADIX raised to that power minus 1
 * can be represented as a normalized value of type double.
 */
#define DBL_MIN_EXP       (2 - __DBL_EBIAS)
/**
 * Minimum negative integer such that FLT_RADIX raised to that power minus 1
 * can be represented as a normalized value of type long double.
 */
#define LDBL_MIN_EXP      (2 - __LDBL_EBIAS)

/**
 * Minimum negative integer such that 10 raised to that power can be
 * represented as a normalized value of type float.
 */
#define FLT_MIN_10_EXP    ((int) ((1 - __FLT_EBIAS) * __LG2))
/**
 * Minimum negative integer such that 10 raised to that power can be
 * represented as a normalized value of type double.
 */
#define DBL_MIN_10_EXP    ((int) ((1 - __DBL_EBIAS) * __LG2))
/**
 * Minimum negative integer such that 10 raised to that power can be
 * represented as a normalized value of type long double.
 */
#define LDBL_MIN_10_EXP   ((int) ((1 - __LDBL_EBIAS) * __LG2))

/**
 * Maximum integer such that FLT_RADIX raised to that power minus 1 can be
 * represented as a finite value of type float.
 */
#define FLT_MAX_EXP       (1 + __FLT_EBIAS)
/**
 * Maximum integer such that FLT_RADIX raised to that power minus 1 can be
 * represented as a finite value of type double.
 */
#define DBL_MAX_EXP       (1 + __DBL_EBIAS)
/**
 * Maximum integer such that FLT_RADIX raised to that power minus 1 can be
 * represented as a finite value of type long double.
 */
#define LDBL_MAX_EXP      (1 + __LDBL_EBIAS)

/**
 * Maximum integer such that 10 raised to that power minus 1 can be represented
 * as a finite value of type float.
 */
#define FLT_MAX_10_EXP    ((int) ((1 + __FLT_EBIAS) * __LG2))
/**
 * Maximum integer such that 10 raised to that power minus 1 can be represented
 * as a finite value of type double.
 */
#define DBL_MAX_10_EXP    ((int) ((1 + __DBL_EBIAS) * __LG2))
/**
 * Maximum integer such that 10 raised to that power minus 1 can be represented
 * as a finite value of type long double.
 */
#define LDBL_MAX_10_EXP   ((int) ((1 + __LDBL_EBIAS) * __LG2))

extern struct __flt_values {
  union {
    unsigned short w[2];
    float          f;
  } max, min, eps;
} __flt;

extern struct __dbl_values {
  union {
    unsigned short w[4];
    double         d;
  } max, min, eps;
} __dbl;

extern struct __ldbl_values {
  union {
    unsigned short w[4];
    long double    ld;
  } max, min, eps;
} __ldbl;

/**
 * Maximum representable finite floating-point number of type float.
 */
#define FLT_MAX           (__flt.max.f)
/**
 * Maximum representable finite floating-point number of type double.
 */
#define DBL_MAX           (__dbl.max.d)
/**
 * Maximum representable finite floating-point number of type long double.
 */
#define LDBL_MAX          (__ldbl.max.ld)

/**
 * The difference between 1 and the least value greater than 1 that is
 * representable in type float.
 */
#define FLT_EPSILON       (__flt.eps.f)
/**
 * The difference between 1 and the least value greater than 1 that is
 * representable in type double.
 */
#define DBL_EPSILON       (__dbl.eps.d)
/**
 * The difference between 1 and the least value greater than 1 that is
 * representable in type long double.
 */
#define LDBL_EPSILON      (__ldbl.eps.ld)

/**
 * Minimum normalized positive floating-point number of type float.
 */
#define FLT_MIN           (__flt.min.f)
/**
 * Minimum normalized positive floating-point number of type double.
 */
#define DBL_MIN           (__dbl.min.d)
/**
 * Minimum normalized positive floating-point number of type long double.
 */
#define LDBL_MIN          (__ldbl.min.ld)

#endif  // !__INCLUDE_FLOAT_H__
