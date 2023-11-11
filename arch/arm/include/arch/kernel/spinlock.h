#ifndef __AG_INCLUDE_ARCH_KERNEL_SPINLOCK_H__
#define __AG_INCLUDE_ARCH_KERNEL_SPINLOCK_H__

#include <stddef.h>
#include <stdint.h>

void arch_spin_lock(volatile int *);
void arch_spin_unlock(volatile int *);
void arch_spin_pcs_save(uintptr_t *, size_t);

#endif  // !__AG_INCLUDE_ARCH_KERNEL_SPINLOCK_H__
