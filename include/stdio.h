#ifndef __STDIO_H__
#define __STDIO_H__

#include <stdarg.h>
#include <stddef.h>
#include <sys/types.h>

/** Size of stdio buffers */
#define BUFSIZ    256

/** Input/output unbuffered */
#define _IONBF    0
/** Input/output line buffered */
#define _IOLBF    1
/** Input/output fully buffered */
#define _IOFBF    2

#define SEEK_SET  0
#define SEEK_CUR  1
#define SEEK_END  2

#define _UNGETC_MAX 2

/** A structure containing information about a file. */
typedef struct _File {
  int    fd;                // File descriptor
  int    mode;              // Mode bits
  int    state;             // State bits

  char  *buf;
  size_t buf_size;    

  char   char_buf[1];       // One-character buffer

  char   back[_UNGETC_MAX]; // Buffer for pushback characters
  size_t back_count;        // The number of pushed back characters

  char  *next;              // Pointer to the next read/write position
  char  *read_end;          // Pointer beyond the last available read position
  char  *read_save;         // Save read end
  char  *write_end;         // Pointer beyond the last available write position
} FILE;

#define _MODE_READ        (1 << 0)  // Open for reading
#define _MODE_WRITE       (1 << 1)  // Open for writing
#define _MODE_APPEND      (1 << 2)  // Append
#define _MODE_CREAT       (1 << 3)  // Create file
#define _MODE_TRUNC       (1 << 4)  // Truncate to zero length
#define _MODE_ALLOC_FILE  (1 << 5)  // Free file on close
#define _MODE_ALLOC_BUF   (1 << 6)  // Free buffer on close
#define _MODE_NO_BUF      (1 << 7)
#define _MODE_LINE_BUF    (1 << 8)
#define _MODE_FULL_BUF    (1 << 9)

#define _STATE_EOF        (1 << 2)
#define _STATE_ERROR      (1 << 3)

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

int   __fflush(FILE *);
void  __ffree(FILE *);
int   __fopen(FILE *, const char *, const char *);
int   __fclose(FILE *);

FILE *fopen(const char *, const char *);
int   fclose(FILE *);
FILE *freopen(const char *, const char *, FILE *);

int   remove(const char *);

int   __printf(int (*)(void *, int), void *, const char *, va_list);

int    feof(FILE *);
int    ferror(FILE *);
int    fgetc(FILE *);
char  *fgets(char *, int, FILE *);
int    fprintf(FILE *, const char *, ...);
int    fputs(const char *, FILE *);
size_t fread(void *, size_t, size_t, FILE *);
size_t fwrite(const void *, size_t, size_t, FILE *);
int    getc(FILE *);
int    getchar(void);
char  *gets(char *);
void   perror(const char *);
int    printf(const char *, ...);
int    putc(int, FILE *);
int    putchar(int);
int    puts(const char *);
void   setbuf(FILE *, char *);
int    snprintf(char *, size_t, const char *, ...);
int    sprintf(char *, const char *, ...);
int    vprintf(const char *, va_list);

// TODO
int    vfprintf(FILE *, const char *, va_list);
int    vsnprintf(char *, size_t , const char *, va_list);
int    vsprintf(char *, const char *, va_list);
int    fflush(FILE *);
size_t fread(void *, size_t, size_t, FILE *);
int    fseek(FILE *, long, int);
long   ftell(FILE *);
size_t fwrite(const void *, size_t, size_t, FILE *);
void   setbuf(FILE *, char *);
int    vfprintf(FILE *, const char *, va_list);
int    setvbuf(FILE *, char *, int, size_t);
int    fsync(int);
int    fileno(FILE *);
int    fputc(int , FILE *);
int    fseeko(FILE *, off_t, int);

#define getc(stream)    (((stream)->next < (stream)->read_end) \
  ? *(stream)->next++ \
  : fgetc(stream))

#define getchar()       (((stdin)->next < (stdin)->read_end) \
  ? *(stdin)->next++ \
  : fgetc(stdin))

#define putc(c, stream) (((stream)->next < (stream)->write_end) \
  ? (*(stream)->next++ = c) \
  : fputc(c, stream))

#define putchar(c)      ((stdout->next < stdout->write_end) \
  ? (*stdout->next++ = c) \
  : fputc(c, stdout))

#ifdef __cplusplus
};
#endif

#endif  // !__STDIO_H__
