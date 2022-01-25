#ifndef __INCLUDE_STDDEF_H__
#define __INCLUDE_STDDEF_H__

/**
 * @file include/stdint.h
 * 
 * Standard type definitions
 */

#include <yvals.h>

/**
 * Signed integer type of the result of subtracting two pointers.
 */
typedef long            ptrdiff_t;

/**
 * Unsigned integer type of the result of the sizeof operator.
 */
typedef unsigned int   size_t;

/**
 * Integer type to represent wide-character codes.
 */
typedef unsigned short  wchar_t;

/**
 * Integer constant expression of type size_t, the value of which is the offset
 * in bytes to the structure member from the beginning of its structure (type).
 */
#define offsetof(type, member)  ((size_t) (&((type *) 0)->member))

#ifndef __cplusplus
  /**
   * Null pointer constant.
   */
  #define NULL    ((void *) 0)
#else
  #define NULL    nullptr
#endif

#endif  // !__INCLUDE_STDDEF_H__
