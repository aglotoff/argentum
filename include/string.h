#ifndef __INCLUDE_STRING_H__
#define __INCLUDE_STRING_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int    memcmp(const void *, const void *, size_t);
void  *memmove(void *, const void *, size_t);
void  *memset(void *, int, size_t);
char  *strchr(const char *, int);
int    strcmp(const char *, const char *);
size_t strlen(const char *);

#ifdef __cplusplus
};
#endif

#endif  // !__INCLUDE_STRING_H__
