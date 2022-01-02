#ifndef __INCLUDE_SETJMP_H__
#define __INCLUDE_SETJMP_H__

/**
 * @file include/setjmp.h
 * 
 * Stack environment declarations
 */

#include <yvals.h>

/**
 * Array to hold information to restore calling environment.
 */
typedef long jmp_buf[__NSETJMP];

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Non-local goto.
 * 
 * Restore the environment saved by the most recent invocation of setjmp with
 * the corresponding jmp_buf argument.
 * 
 * @param env Object containing the environment to be restored.
 * @param val The value to be returned from setjmp. If 0, setjmp returns 1.
 *
 * @sa setjmp
 */
void longjmp(jmp_buf env, int val);

/**
 * Set jump point for a non-local goto.
 * 
 * @param env Object where the calling environment is stored.
 * 
 * @return 0 on the direct invocation; a non-zero value if the return is from a
 *         call to longjmp.
 *
 * @sa longjmp
 */
int setjmp(jmp_buf env);

#ifdef __cplusplus
};
#endif

#endif // !__INCLUDE_SETJMP_H__
