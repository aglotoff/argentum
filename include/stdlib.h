#ifndef __INCLUDE_STDLIB_H__
#define __INCLUDE_STDLIB_H__

#define EXIT_SUCCESS  0   ///< Successful termination status
#define EXIT_FAILURE  1   ///< Unsuccessful termination status

#ifdef __cplusplus
extern "C" {
#endif

void exit(int);

#ifdef __cplusplus
};
#endif

#endif  // !__INCLUDE_STDLIB_H__
