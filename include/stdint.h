#ifndef __AG_INCLUDE_STDINT_H__
#define __AG_INCLUDE_STDINT_H__

/**
 * @file include/stdint.h
 * 
 * Integer types
 */

/** Signed integer type with a width of exactly 8 bits */
typedef signed char         int8_t;
/** Signed integer type with a width of exactly 8 bits */
typedef unsigned char       uint8_t;
/** Signed integer type with a width of exactly 16 bits */
typedef short               int16_t;
/** Signed integer type with a width of exactly 16 bits */ 
typedef unsigned short      uint16_t;
/** Signed integer type with a width of exactly 32 bits */ 
typedef int                 int32_t;
/** Signed integer type with a width of exactly 32 bits */ 
typedef unsigned int        uint32_t;
/** Signed integer type with a width of exactly 64 bits */ 
typedef long long           int64_t;
/** Signed integer type with a width of exactly 64 bits */ 
typedef unsigned long long  uint64_t;

/** Signed integer type wide enough to hold any valid pointer */
typedef long                intptr_t;
/** Unsigned integer type wide enough to hold any valid pointer */
typedef unsigned long       uintptr_t;

#endif  // !__AG_INCLUDE_STDINT_H__
