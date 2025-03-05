#ifndef __KERNEL_INCLUDE_KERNEL_SPINLOCK_H__
#define __KERNEL_INCLUDE_KERNEL_SPINLOCK_H__

#include <kernel/core/config.h>

struct KCpu;

/** The maximum depth of call stack that could be recorded by a spinlock */
#define SPIN_MAX_PCS  10

/**
 * Spinlocks provide mutual exclusion, ensuring only one CPU at a time can hold
 * the lock. A task trying to acquire the lock waits in a loop repeatedly
 * testing the lock until it becomes available.
 *
 * Spinlocks are used if the holding time is short or if the data to be
 * protected is accessed from an interrupt handler context.
 */
struct KSpinLock {
  /** Whether the spinlock is held */
  volatile int  locked;

  /** The CPU holding this spinlock */
  struct KCpu  *cpu;
  /** Spinlock name (for debugging purposes) */
  const char   *name;
  /** Saved call stack (an array of program counters) that locked the lock */
  uintptr_t     pcs[SPIN_MAX_PCS];
};

/**
 * Initialize a static spinlock.
 * 
 * @param name The name of the spinlock
 */
#define K_SPINLOCK_INITIALIZER(spin_name) { \
  .locked = 0,                              \
  .cpu    = NULL,                           \
  .name   = (spin_name),                    \
  .pcs    = { 0 }                           \
}

void k_spinlock_init(struct KSpinLock *, const char *);
void k_spinlock_acquire(struct KSpinLock *);
void k_spinlock_release(struct KSpinLock *);
int  k_spinlock_holding(struct KSpinLock *);

void k_arch_spinlock_acquire(volatile int *);
void k_arch_spinlock_release(volatile int *);
void k_arch_spinlock_save_callstack(struct KSpinLock *);
void k_arch_spinlock_print_callstack(struct KSpinLock *);

#endif  // !__KERNEL_INCLUDE_KERNEL_SPINLOCK_H__
