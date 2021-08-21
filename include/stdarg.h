#ifndef INCLUDE_STDARG_H
#define INCLUDE_STDARG_H

/**
 * Type to hold information about variable arguments.
 */
typedef __builtin_va_list va_list;

/** 
 * Initialize a variable argument list. 
 *
 * Initialize ap to retrieve additional arguments after parameter argN.
 * 
 * @param ap Uninitialized object of type va_list.
 * @param argN Name of the last parameter in the function definition.
 */
#define va_start(ap, argN)  __builtin_va_start(ap, argN)

/** 
 * Copy variable argument list.
 *
 * @param dest Uninitialized object of type va_list.
 * @param src va_list object previously initialized with va_start.
 */
#define va_copy(dest, src)  __builtin_va_copy(dest, src)

/** 
 * Retrieve next argument.
 *
 * @param ap va_list object reviously initialized with va_start.
 * @param type A type name.
 *
 * @return Next additional argument of given type.
 */
#define va_arg(ap, type)    __builtin_va_arg(ap, type)

/** 
 * End using variable argument list.
 *
 * @param ap va_list object previously initialized with va_start.
 */
#define va_end(ap)          __builtin_va_end(ap)

#endif  // !INCLUDE_STDARG_H
