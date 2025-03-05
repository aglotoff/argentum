#ifndef _KERNEL_CORE_ASSERT_H_
#define _KERNEL_CORE_ASSERT_H_

#include <kernel/core/config.h>

#ifdef NDEBUG
  #define k_assert(ignore)  ((void) 0)
#else
  /**
   * Evaluate assertion.
   * 
   * @param expr The expression to be evaluated.
   */
  #define k_assert(expr) \
    do { if (!(expr)) k_panic("Assertion failed: %s", #expr); } while(0)
#endif

#endif  // !_KERNEL_CORE_ASSERT_H_
