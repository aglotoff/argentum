#ifndef __STDINT_H__
#define __STDINT_H__

/**
 * @file include/stdint.h
 * 
 * Integer types
 */

#include <yvals.h>

typedef signed char         int8_t;     ///< Signed integer of 8-bit width
typedef unsigned char       uint8_t;    ///< Unsigned integer of 8-bit width
typedef short               int16_t;    ///< Signed integer of 16-bit width
typedef unsigned short      uint16_t;   ///< Unsigned integer of 16-bit width
#if __INT_LONG
  typedef int               int32_t;    ///< Signed integer of 32-bit width
  typedef unsigned          uint32_t;   ///< Unsigned integer of 32-bit width
#else
  typedef long              int32_t;
  typedef unsigned long     uint32_t;
#endif
typedef long long           int64_t;    ///< Signed integer of 64-bit width
typedef unsigned long long  uint64_t;   ///< Unsigned integer of 64-bit width

/** Signed integer wide enough to hold a pointer */
typedef long                intptr_t;

/** Unsigned integer wide enough to hold a pointer */
typedef unsigned long       uintptr_t;

#endif  // !__STDINT_H__
