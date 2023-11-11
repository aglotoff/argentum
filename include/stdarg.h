#ifndef __AG_INCLUDE_STDARG_H__
#define __AG_INCLUDE_STDARG_H__

/**
 * @file include/stdarg.h
 */

/** Type to hold information about variable arguments */
typedef __builtin_va_list va_list;

/** Initialize a variable argument list */
#define va_start(ap, argN)  __builtin_va_start(ap, argN)

/** Copy a variable argument list */
#define va_copy(dest, src)  __builtin_va_copy(dest, src)

/** Retrieve the next argument from a variable argument list */
#define va_arg(ap, type)    __builtin_va_arg(ap, type)

/** End using a variable argument list */
#define va_end(ap)          __builtin_va_end(ap)

#endif  // !__AG_INCLUDE_STDARG_H__
