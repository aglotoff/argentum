#ifndef __INCLUDE_STDINT_H__
#define __INCLUDE_STDINT_H__

/**
 * @file include/stdint.h
 * 
 * Integer types
 */

#include <yvals.h>

typedef __int8_t    int8_t;     ///< Signed integer of 8-bit width
typedef __uint8_t   uint8_t;    ///< Unsigned integer of 8-bit width
typedef __int16_t   int16_t;    ///< Signed integer of 16-bit width
typedef __uint16_t  uint16_t;   ///< Unsigned integer of 16-bit width
typedef __int32_t   int32_t;    ///< Signed integer of 32-bit width
typedef __uint32_t  uint32_t;   ///< Unsigned integer of 32-bit width
typedef __int64_t   int64_t;    ///< Signed integer of 64-bit width
typedef __uint64_t  uint64_t;   ///< Unsigned integer of 64-bit width

/** Signed integer wide enough to hold a pointer */
typedef __intptr_t  intptr_t;

/** Unsigned integer wide enough to hold a pointer */
typedef __uintptr_t uintptr_t;

#endif  // !__INCLUDE_STDINT_H__
