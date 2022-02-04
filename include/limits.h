#ifndef __LIMITS_H__
#define __LIMITS_H__

/**
 * @file include/limits.h
 * 
 * Implementation-defined constants.
 */

#include <yvals.h>

// Minimum values
#define _POSIX_ARG_MAX      4096
#define _POSIX_CHILD_MAX    6
#define _POSIX_LINK_MAX     8
#define _POSIX_MAX_CANON    255
#define _POSIX_MAX_INPUT    255
#define _POSIX_NAME_MAX     14
#define _POSIX_NGROUPS_MAX  0
#define _POSIX_OPEN_MAX     20
#define _POSIX_PATH_MAX     255
#define _POSIX_PIPE_BUF     512

// Runtime Invariant Values
#define ARG_MAX             4096
#define ATEXIT_MAX          32
#define CHILD_MAX           6
#define LINK_MAX            8
#define MAX_CANON           255
#define MAX_INPUT           255
#define NAME_MAX            255
#define OPEN_MAX            32
#define PATH_MAX            4096
#define PIPE_BUF            4096

/** The number of bits in the char data type. */
#define CHAR_BIT          8

/** The minimum value of the signed char data type. */
#define SCHAR_MIN         (-127 - __C2)
/** The maximum value of the signed char data type. */
#define SCHAR_MAX         127

/** The maximum value of the unsigned char data type. */
#define UCHAR_MAX         255

#if __CHAR_SIGN
  /** The minimum value of the char data type. */
  #define CHAR_MIN        SCHAR_MIN
  /** The maximum value of the char data type. */
  #define CHAR_MAX        SCHAR_MAX
#else
  #define CHAR_MIN        0
  #define CHAR_MAX        UCHAR_MAX
#endif

/** The maximum number of bytes in a character, for any supported locale. */
#define MB_LEN_MAX        __MB_LEN_MAX

/** The minimum value of the short int data type. */
#define SHRT_MIN          (-32767 - __C2)
/** The maximum value of the short int data type. */
#define SHRT_MAX          32767
/** The maximum value of the unsigned short int data type. */
#define USHRT_MAX         65535

#if __INT_LONG
  /** The minimum value of the int data type. */
  #define INT_MIN         (-2147483647 - __C2)
  /** The maximum value of the int data type. */
  #define INT_MAX         2147483647
  /** The maximum value of the unsigned int data type. */
  #define UINT_MAX        4294967295U
#else
  #define INT_MIN         (-32767 - __C2)
  #define INT_MAX         32767
  #define UINT_MAX        65535
#endif

#if __LONG_LONG
  /** The minimum value of the long int data type. */
  #define LONG_MIN        -9223372036854775807L
  /** The maximum value of the long int data type. */
  #define LONG_MAX        9223372036854775807L
  /** The maximum value of the unsigned long int data type. */
  #define ULONG_MAX       18446744073709551615UL
#else
  #define LONG_MIN        (-2147483647L - __C2)
  #define LONG_MAX        2147483647L
  #define ULONG_MAX       4294967295UL
#endif

/** The minimum value of the long long int data type. */
#define LLONG_MIN         -9223372036854775807LL
/** The maximum value of the long long int data type. */
#define LLONG_MAX         9223372036854775807LL
/** The maximum value of the unsigned long long int data type. */
#define ULLONG_MAX        18446744073709551615ULL

#endif  // !__LIMITS_H__
