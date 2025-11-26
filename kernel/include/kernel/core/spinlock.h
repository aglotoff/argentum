#ifndef __KERNEL_INCLUDE_CORE_SPINLOCK_H__
#define __KERNEL_INCLUDE_CORE_SPINLOCK_H__

#include <kernel/core/config.h>

struct KCpu;

/**
 * @brief Represents a kernel spinlock object.
 *
 * A spinlock ensures mutual exclusion between CPUs. It can be used to protect
 * shared kernel data structures in contexts where sleeping is not allowed.
 */
struct KSpinLock {
  volatile int  locked;

  struct KCpu  *cpu;
  const char   *name;
  k_uintptr_t   pcs[K_SPINLOCK_MAX_PCS];
};

/**
 * @brief Static initializer for a spinlock.
 *
 * Provides a compile-time initialization macro that sets up a spinlock in the
 * unlocked state with the specified name.
 *
 * @param spin_name Human-readable name for the spinlock (const char *).
 */
#define K_SPINLOCK_INITIALIZER(spin_name) { \
  .locked = 0,                              \
  .cpu    = K_NULL,                         \
  .name   = (spin_name),                    \
  .pcs    = { 0 }                           \
}

/* -------------------------------------------------------------------------- */
/*                                 Kernel API                                 */
/* -------------------------------------------------------------------------- */

void k_spinlock_init(struct KSpinLock *, const char *);
void k_spinlock_acquire(struct KSpinLock *);
void k_spinlock_release(struct KSpinLock *);
int  k_spinlock_holding(struct KSpinLock *);

/* -------------------------------------------------------------------------- */
/*                           Architecture Interface                           */
/* -------------------------------------------------------------------------- */

/**
 * @brief Acquire a low-level hardware spinlock.
 *
 * Atomically acquires the spinlock flag at the architecture level.
 *
 * @param lock Pointer to the architecture-specific lock variable.
 *
 * @note Implemented per architecture (e.g., using atomic test-and-set or
 *       compare-and-swap instructions).
 */
void k_arch_spinlock_acquire(volatile int *);

/**
 * @brief Release a low-level hardware spinlock.
 *
 * Atomically releases the spinlock flag at the architecture level.
 *
 * @param lock Pointer to the architecture-specific lock variable.
 */
void k_arch_spinlock_release(volatile int *);

#endif  // !__KERNEL_INCLUDE_CORE_SPINLOCK_H__
