#ifndef __INCLUDE_KERNEL_CORE_ASSERT_H__
#define __INCLUDE_KERNEL_CORE_ASSERT_H__

#include <kernel/core/config.h>

#ifdef K_NDEBUG
  #define k_assert(ignore)  ((void) 0)
#else
  /**
   * @brief Verify a runtime condition in debug builds.
   *
   * Evaluates the expression `expr` and triggers a kernel panic if it
   * evaluates to false. This macro is intended for use inside kernel code
   * to validate internal assumptions, preconditions, and invariants that
   * must hold true for correct operation.
   *
   * In release builds (compiled with `K_NDEBUG` defined), this macro expands
   * to a no-op and has no runtime effect.
   *
   * @param expr The expression to test. If false, a panic is triggered.
   */
  #define k_assert(expr) \
    do { if (!(expr)) k_panic("Assertion failed: %s", #expr); } while(0)
#endif

#endif  // !__INCLUDE_KERNEL_CORE_ASSERT_H__
