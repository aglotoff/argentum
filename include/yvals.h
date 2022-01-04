#ifndef __INCLUDE_YVALS_H__
#define __INCLUDE_YVALS_H__

/**
 * @file include/yvals.h
 * 
 * Compiler-dependent parameters.
 */

// Whether the encoding is two's complement
#define __C2          1

// Whether a char is signed
#define __CHAR_SIGN   1

// Whether an int has 4 bytes 
#define __INT_LONG    1

// Whether a long has 8 bytes
#define __LONG_LONG   0

// The maximum number of bytes in a character, for any supported locale
#define __MB_LEN_MAX  1

// Subscripts of words in floating-point representations.
// D0 is the most significant word.
#define __D0		      3
#define __D1		      (D0 - 1)
#define __D2		      (D1 - 1)
#define __D3		      (D2 - 1)

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

// Whether long double has the IEEE 754 80-bit format
#define __LDBL        0

// The rounding mode for floating-point addition
#define __FLT_ROUNDS  1

// The number of elements in jmp_buf (we must store R4-R11, IP, SP, and LR)
#define __NSETJMP   11

#endif  // !__INCLUDE_YVALS_H__
