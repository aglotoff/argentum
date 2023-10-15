#ifndef __AG_STDIO_H__
#define __AG_STDIO_H__

/**
 * @file include/stdio.h
 */

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int  snprintf(char *, size_t , const char *, ...);
int  vsnprintf(char *, size_t, const char *, va_list);
void xprintf(void (*)(void *, int),  void *, const char *, va_list);

#ifdef __cplusplus
};
#endif

#endif  // !__AG_STDIO_H__
