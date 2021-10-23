#ifndef __KERNEL_CONST_H__
#define __KERNEL_CONST_H__

/**
 * @file kernel/kernel.h
 *
 * Defines some often used types and macros.
 */

#include <stddef.h>
#include <stdint.h>

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

#endif  // !__KERNEL_CONST_H__
