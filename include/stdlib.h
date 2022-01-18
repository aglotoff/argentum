#ifndef __INCLUDE_STDLIB_H__
#define __INCLUDE_STDLIB_H__

/**
 * @file include/stdlib.h
 * 
 * General utilities.
 */

#define EXIT_SUCCESS  0   ///< Successful termination status
#define EXIT_FAILURE  1   ///< Unsuccessful termination status

#include <stddef.h>
#include <limits.h>
#include <sys/wait.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Type returned by the div function.
 */
typedef struct {
  int quot;     ///< Quotient
  int rem;      ///< Remainder
} div_t;

/**
 * Type returned by the ldiv function.
 */
typedef struct {
  long quot;    ///< Quotient
  long rem;     ///< Remainder
} ldiv_t;

void   abort(void);
void   exit(int);
char  *getenv(const char *);

void  *bsearch(const void *, const void *, size_t, size_t,
               int (*)(const void *, const void *));

int    abs(int);
div_t  div(int, int);
long   labs(long);
ldiv_t ldiv(long, long);

#ifdef __cplusplus
};
#endif

#endif  // !__INCLUDE_STDLIB_H__
