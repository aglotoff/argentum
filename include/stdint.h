#ifndef __INCLUDE_STDINT_H__
#define __INCLUDE_STDINT_H__

// Exact-width integer types
typedef signed char         int8_t;     ///< Signed integer of 8-bit width
typedef unsigned char       uint8_t;    ///< Unsigned integer of 8-bit width
typedef short               int16_t;    ///< Signed integer of 16-bit width
typedef unsigned short      uint16_t;   ///< Unsigned integer of 16-bit width
typedef int                 int32_t;    ///< Signed integer of 32-bit width
typedef unsigned            uint32_t;   ///< Unsigned integer of 32-bit width
typedef long long           int64_t;    ///< Signed integer of 64-bit width
typedef unsigned long long  uint64_t;   ///< Unsigned integer of 64-bit width

/** Signed integer wide enough to hold a pointer */
typedef long            intptr_t;

/** Unsigned integer wide enough to hold a pointer */
typedef unsigned long   uintptr_t;

#endif  // !__INCLUDE_STDINT_H__
