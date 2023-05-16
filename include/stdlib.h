#ifndef __STDLIB_H__
#define __STDLIB_H__

/**
 * @file include/stdlib.h
 * 
 * General utilities.
 */

#include <stddef.h>
#include <limits.h>
#include <sys/wait.h>

/** Successful termination status */
#define EXIT_SUCCESS  0
/** Unsuccessful termination status */
#define EXIT_FAILURE  1

/** Maximum value returned by rand(). */
#define RAND_MAX      0x7fffffff

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Type returned by the div function.
 */
typedef struct {
  /** Quotient */
  int quot;
  /** Remainder */
  int rem;
} div_t;

/**
 * Type returned by the ldiv function.
 */
typedef struct {
  /** Quotient */
  long quot;
  /** Remainder */
  long rem;
} ldiv_t;

// The seed for the pseudo-random sequence generator
extern unsigned __stdlib_seed;

struct __BlkHeader {
  struct __BlkHeader *next; // Next block (if on the free list)
  size_t              size; // Size of this block (in bytes)
};

extern struct __BlkHeader *__alloc_free; // Start of the free list

extern void   (*__at_funcs[])(void);
extern size_t   __at_count;

#define __STDLIB_PARSE_INT_SIGNED     (1 << 0)
#define __STDLIB_PARSE_INT_LONGLONG   (1 << 1)

int                 atoi(const char *);
long                atol(const char *);
long                strtol(const char *, char **, int);
unsigned long       strtoul(const char *, char **, int);
unsigned long long  __stdlib_parse_int(const char *, char **, int, int);

int                 rand(void);
int                 rand_r(unsigned *);
void                srand(unsigned);

void                _Exit(int);
int                 atexit(void (*)(void));
void                abort(void);
void                exit(int);
char               *getenv(const char *);
int                 setenv(const char *, const char *, int);

void               *bsearch(const void *, const void *, size_t, size_t,
                            int (*)(const void *, const void *));
void                qsort(void *, size_t, size_t,
                          int (*)(const void *, const void *));

int                 abs(int);
div_t               div(int, int);
long                labs(long);
ldiv_t              ldiv(long, long);

struct __BlkHeader *__getmem(size_t);
void               *calloc(size_t, size_t);
void               *malloc(size_t);
void                free(void *);
void               *realloc(void *, size_t);
int                 wctomb(char *, wchar_t);

// TODO: atof, strtod

#ifdef __cplusplus
};
#endif

#endif  // !__STDLIB_H__
