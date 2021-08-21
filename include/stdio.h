#ifndef INCLUDE_STDIO_H
#define INCLUDE_STDIO_H

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generic function to print formatted data.
 * 
 * This is a simplified printf-like implementation that supports the following
 * subset of features:
 * - Conversion specifiers: d, u, o, x, p, c, s
 * - Length modifiers: hh, h, l, ll
 * - Width and precision fields
 *
 * @param xputc Pointer to the output function.
 * @param xputc_arg The first argument for the output function.
 * @param format The format string.
 * @param ap A variable argument list.
 */
void 
xprintf(void      (*xputc)(void *, int),
        void       *xputc_arg,
        const char *format,
        va_list     ap);

#ifdef __cplusplus
};
#endif

#endif  // !INCLUDE_STDIO_H
