#ifndef __AG_INCLUDE_KERNEL_KERNEL_H__
#define __AG_INCLUDE_KERNEL_KERNEL_H__

#ifndef __ASSEMBLER__

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

extern const char *panic_str;

/**
 * Get the minimum of 'a' and 'b'.
 */
#define MIN(a, b)         \
({                        \
  typeof(a) _a = (a);     \
  typeof(b) _b = (b);     \
  (_a <= _b) ? _a : _b;   \
})

/**
 * Get the maximum of 'a' and 'b'.
 */
#define MAX(a, b)         \
({                        \
  typeof(a) _a = (a);     \
  typeof(b) _b = (b);     \
  (_a >= _b) ? _a : _b;   \
})

/**
 * Round 'x' down to the nearset multiple of 'n'.
 */
#define ROUND_DOWN(x, n)                                    \
({                                                          \
  unsigned long _x = (unsigned long) (x);                   \
  (typeof(x)) (_x - (_x % (n)));                            \
})

/**
 * Round 'x' up to the nearset multiple of 'n'.
 */
#define ROUND_UP(x, n)                                      \
({                                                          \
  unsigned long _n = (unsigned long) (n);                   \
  (typeof(x)) ROUND_DOWN((unsigned long) x + _n - 1, _n);   \
})

/**
 * Determine the size of the static array `a`.
 */
#define ARRAY_SIZE(a)   (sizeof(a) / sizeof(a[0]))

#ifdef __cplusplus
extern "C" {
#endif

void kprintf(const char *, ...);
void vkprintf(const char *, va_list);

#ifdef __cplusplus
};
#endif

#endif  // !__ASSEMBLER__

#endif  // !__AG_INCLUDE_KERNEL_KERNEL_H__
