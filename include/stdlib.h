#ifndef __INCLUDE_STDLIB_H__
#define __INCLUDE_STDLIB_H__

/**
 * @file include/stdlib.h
 * 
 * Standard library definitions.
 */

#define EXIT_SUCCESS  0   ///< Successful termination status
#define EXIT_FAILURE  1   ///< Unsuccessful termination status

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void  abort(void);
void  exit(int);
char *getenv(const char *);

#ifdef __cplusplus
};
#endif

#endif  // !__INCLUDE_STDLIB_H__
