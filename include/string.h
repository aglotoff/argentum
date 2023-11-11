#ifndef __AG_INCLUDE_STRING_H__
#define __AG_INCLUDE_STRING_H__

/**
 * @file include/string.h
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void  *memmove(void *, const void *, size_t);
void  *memset(void *, int, size_t);
char  *strchr(const char *, int);
size_t strlen(const char *);
char  *strncpy(char *, const char *, size_t);

#ifdef __cplusplus
};
#endif

#endif  // !__AG_INCLUDE_STRING_H__
