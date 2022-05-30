#ifndef __STDIO_H__
#define __STDIO_H__

#include <stdarg.h>
#include <stddef.h>

/** A structure containing information about a file. */
typedef struct {
  int _Mode;
  int _Fildes;
} FILE;

#define _MODE_READ    (1 << 0)  // Open for reading
#define _MODE_WRITE   (1 << 1)  // Open for writing
#define _MODE_APPEND  (1 << 2)  // Append
#define _MODE_CREAT   (1 << 3)  // Create file
#define _MODE_TRUNC   (1 << 4)  // Truncate to zero length

/** The number of streams that can be open simultaneously. */
#define FOPEN_MAX   32

/** End-of-file return value. */
#define EOF -1

extern FILE *__files[];

#define stdin   __files[0]    ///< Standard input stream
#define stdout  __files[1]    ///< Standard output stream
#define stderr  __files[2]    ///< Standard error output stream

#ifdef __cplusplus
extern "C" {
#endif

FILE *fopen(const char *, const char *);
int   fclose(FILE *);

int   remove(const char *);

int   __printf(int (*)(void *, int), void *, const char *, va_list);
int   printf(const char *, ...);
int   snprintf(char *s, size_t n, const char *, ...);
int   vprintf(const char *, va_list);
int   vsnprintf(char *s, size_t n, const char *, va_list);

void  perror(const char *);

#ifdef __cplusplus
};
#endif

#endif  // !__STDIO_H__
