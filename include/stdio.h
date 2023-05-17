#ifndef __STDIO_H__
#define __STDIO_H__

#include <stdarg.h>
#include <stddef.h>

#define SEEK_SET  0
#define SEEK_CUR  1
#define SEEK_END  2

/** A structure containing information about a file. */
typedef struct _File {
  int mode;       // Mode bits
  int fd;         // File descriptor
} FILE;

#define _MODE_READ        (1 << 0)  // Open for reading
#define _MODE_WRITE       (1 << 1)  // Open for writing
#define _MODE_APPEND      (1 << 2)  // Append
#define _MODE_CREAT       (1 << 3)  // Create file
#define _MODE_TRUNC       (1 << 4)  // Truncate to zero length
#define _MODE_ALLOC_FILE  (1 << 5)  // Free file on close

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

void  __ffree(FILE *);
int   __fopen(FILE *, const char *, const char *);
int   __fclose(FILE *);
FILE *fopen(const char *, const char *);
int   fclose(FILE *);
FILE *freopen(const char *, const char *, FILE *);

int   remove(const char *);

int   __printf(int (*)(void *, int), void *, const char *, va_list);
int   fprintf(FILE *, const char *, ...);
int   printf(const char *, ...);
int   snprintf(char *s, size_t n, const char *, ...);
int   vprintf(const char *, va_list);
int   vfprintf(FILE *, const char *, va_list);
int   vsnprintf(char *, size_t , const char *, va_list);
int   vsprintf(char *, const char *, va_list);

void  perror(const char *);

// TODO:
int    fflush(FILE *);
size_t fread(void *, size_t, size_t, FILE *);
int    fseek(FILE *, long, int);
long   ftell(FILE *);
size_t fwrite(const void *, size_t, size_t, FILE *);
void   setbuf(FILE *, char *);
int    vfprintf(FILE *, const char *, va_list);
int    sprintf(char *, const char *, ...);
int    setvbuf(FILE *, char *, int, size_t);
int    fsync(int);
int    putc(int, FILE *);
int    fileno(FILE *);
int    getchar(void);

#ifdef __cplusplus
};
#endif

#endif  // !__STDIO_H__
