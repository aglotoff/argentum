#ifndef __INCLUDE_STDIO_H__
#define __INCLUDE_STDIO_H__

#include <stdarg.h>
#include <stddef.h>

#define EOF -1

#ifdef __cplusplus
extern "C" {
#endif

void xprintf(void (*)(void *, int), void *, const char *, va_list);
int  printf(const char *, ...);
int  snprintf(char *s, size_t n, const char *, ...);
int  vprintf(const char *, va_list);
int  vsnprintf(char *s, size_t n, const char *, va_list);

void perror(const char *);

#ifdef __cplusplus
};
#endif

#endif  // !__INCLUDE_STDIO_H__
