#ifndef __KERNEL_SP804_H__
#define __KERNEL_SP804_H__

#include <stdint.h>

/**
 * Sp804 Driver instance.
 */
struct Sp804 {
  volatile uint32_t *base;    ///< Memory base address
};

int  sp804_init(struct Sp804 *, void *);
void sp804_eoi(struct Sp804 *);

#endif  // !__KERNEL_SP804_H__
