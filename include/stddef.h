#ifndef __AG_STDDEF_H__
#define __AG_STDDEF_H__

/**
 * @file include/stddef.h
 * 
 * Standard type definitions
 */

/** Signed integer type of the result of subtracting two pointers */
typedef long            ptrdiff_t;

/** Unsigned integer type of the result of the sizeof operator */
typedef unsigned int   size_t;

/** The offset (in bytes) of a structure member */
#define offsetof(type, member)  ((size_t) (&((type *) 0)->member))

#ifndef __cplusplus
  /**
   * Null pointer constant.
   */
  #define NULL    ((void *) 0)
#else
  #define NULL    nullptr
#endif

#endif  // !__AG_STDDEF_H__
