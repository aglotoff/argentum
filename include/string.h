#ifndef __INCLUDE_STRING_H__
#define __INCLUDE_STRING_H__

/**
 * @file include/string.h
 * 
 * String operations.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Copying functions
void  *memcpy(void *, const void *, size_t);
void  *memmove(void *, const void *, size_t);
char  *strcpy(char *, const char *);
char  *strncpy(char *, const char *, size_t);

// Concatenation functions
char  *strcat(char *, const char *);
char  *strncat(char *, const char *, size_t);

// Comparison functions
int    memcmp(const void *, const void *, size_t);
int    strcmp(const char *, const char *);
// TODO: strcoll
int    strncmp(const char *, const char *, size_t);
// TODO: strxfrm

// Search functions
void * memchr(const void *, int, size_t);
char  *strchr(const char *, int);
size_t strcspn(const char *, const char *);
char  *strpbrk(const char *, const char *);
char  *strrchr(const char *, int);
size_t strspn(const char *, const char *);
char  *strstr(const char *, const char *);
char  *strtok(char *, const char *);

// Miscellaneous functions
void  *memset(void *, int, size_t);
char  *strerror(int);
size_t strlen(const char *);

#ifdef __cplusplus
};
#endif

#endif  // !__INCLUDE_STRING_H__
