#ifndef __INCLUDE_ARGENTUM_SPINLOCK_H__
#define __INCLUDE_ARGENTUM_SPINLOCK_H__

#ifndef __AG_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <stdint.h>

struct Cpu;

/** The maximum depth of call stack that could be recorded by a spinlock */
#define SPIN_MAX_PCS  10

/**
 * Spinlocks provide mutual exclusion, ensuring only one CPU at a time can hold
 * the lock. A thread trying to acquire the lock waits in a loop repeatedly
 * testing the lock until it becomes available.
 *
 * Spinlocks are used if the holding time is short or if the data to be
 * protected is accessed from an interrupt handler context.
 */
struct SpinLock {
  /** Whether the spinlock is held */
  volatile int  locked;

  /** The CPU holding this spinlock */
  struct Cpu   *cpu;
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
#define SPIN_INITIALIZER(spin_name) { \
  .locked = 0,                        \
  .cpu    = NULL,                     \
  .name   = (spin_name),              \
  .pcs    = { 0 }                     \
}

void spin_init(struct SpinLock *, const char *);
void spin_lock(struct SpinLock *);
void spin_unlock(struct SpinLock *);
int  spin_holding(struct SpinLock *);

#endif  // !__INCLUDE_ARGENTUM_SPINLOCK_H__
