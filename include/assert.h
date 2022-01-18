#ifndef __INCLUDE_ASSERT_H__
#define __INCLUDE_ASSERT_H__

#ifdef __cplusplus
extern "C" {
#endif

void __panic(const char *, int, const char *, ...);
void __warn(const char *, int, const char *, ...);

#ifdef __cplusplus
};
#endif

/**
 * Called on unresolveable fatal errors.
 * 
 * Prints an error message and stops execution.
 */
#define panic(...) __panic(__FILE__, __LINE__, __VA_ARGS__)

/**
 * Same as panic but doesn't stop execution.
 */
#define warn(...)  __warn(__FILE__, __LINE__, __VA_ARGS__)

#ifdef NDEBUG
  #define assert(ignore)  ((void) 0)
#else
  /**
   * Evaluate assertion.
   * 
   * @param expr The expression to be evaluated.
   */
  #define assert(expr) \
    do { if (!(expr)) panic("Assertion failed: %s", #expr); } while(0)
#endif

#endif  // !__INCLUDE_ASSERT_H__
