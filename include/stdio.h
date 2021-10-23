#ifndef INCLUDE_STDIO_H
#define INCLUDE_STDIO_H

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void xprintf(void (*)(void *, int), void *, const char *, va_list);
int  printf(const char *, ...);
int  vprintf(const char *, va_list);

#ifdef __cplusplus
};
#endif

#endif  // !INCLUDE_STDIO_H
