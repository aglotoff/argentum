#ifndef __INCLUDE_LIMITS_H__
#define __INCLUDE_LIMITS_H__

/**
 * @file include/limits.h
 * 
 * Implementation-defined constants.
 */

/** The number of bits in the char data type. */
#define CHAR_BIT        8

/** The minimum value of the signed char data type. */
#define SCHAR_MIN       -128
/** The maximum value of the signed char data type. */
#define SCHAR_MAX       127

/** The maximum value of the unsigned char data type. */
#define UCHAR_MAX       255

/** The minimum value of the char data type. */
#define CHAR_MIN        SCHAR_MIN
/** The maximum value of the char data type. */
#define CHAR_MAX        SCHAR_MAX

/** The maximum number of bytes in a character, for any supported locale. */
#define MB_LEN_MAX      1

/** The minimum value of the short int data type. */
#define SHRT_MIN        -32768
/** The maximum value of the short int data type. */
#define SHRT_MAX        32767

/** The maximum value of the unsigned short int data type. */
#define USHRT_MAX       65535

/** The minimum value of the int data type. */
#define INT_MIN         -2147483648
/** The maximum value of the int data type. */
#define INT_MAX         2147483647

/** The maximum value of the unsigned int data type. */
#define UINT_MAX        4294967295U

/** The minimum value of the long int data type. */
#define LONG_MIN        -2147483648L
/** The maximum value of the long int data type. */
#define LONG_MAX        2147483647L

/** The maximum value of the unsigned long int data type. */
#define ULONG_MAX       4294967295UL

/** The minimum value of the long long int data type. */
#define LLONG_MIN       -9223372036854775807LL
/** The maximum value of the long long int data type. */
#define LLONG_MAX       9223372036854775807LL

/** The maximum value of the unsigned long long int data type. */
#define ULLONG_MAX      18446744073709551615ULL

#endif  // !__INCLUDE_LIMITS_H__
