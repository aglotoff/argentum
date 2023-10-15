#ifndef __AG_ASSERT_H__
#define __AG_ASSERT_H__

#ifdef __cplusplus
extern "C" {
#endif

void _Panic(const char *, int, const char *, ...);

#ifdef __cplusplus
};
#endif

/**
 * Print an error message and stop execution. Called on unresolveable errors. 
 */
#define panic(...) _Panic(__FILE__, __LINE__, __VA_ARGS__)

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

#endif  // !__AG_ASSERT_H__
