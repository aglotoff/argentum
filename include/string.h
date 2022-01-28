#ifndef __STRING_H__
#define __STRING_H__

/**
 * @file include/string.h
 * 
 * String operations.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void  *memcpy(void *, const void *, size_t);
void  *memmove(void *, const void *, size_t);
char  *strcpy(char *, const char *);
char  *strncpy(char *, const char *, size_t);

char  *strcat(char *, const char *);
char  *strncat(char *, const char *, size_t);

int    memcmp(const void *, const void *, size_t);
int    strcmp(const char *, const char *);
// TODO: strcoll
int    strncmp(const char *, const char *, size_t);
// TODO: strxfrm

void * memchr(const void *, int, size_t);
char  *strchr(const char *, int);
size_t strcspn(const char *, const char *);
char  *strpbrk(const char *, const char *);
char  *strrchr(const char *, int);
size_t strspn(const char *, const char *);
char  *strstr(const char *, const char *);
char  *strtok(char *, const char *);

void  *memset(void *, int, size_t);
char  *strerror(int);
size_t strlen(const char *);

#ifdef __cplusplus
};
#endif

#endif  // !__STRING_H__
