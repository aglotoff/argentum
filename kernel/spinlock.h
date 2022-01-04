#ifndef __KERNEL_SPINLOCK_H__
#define __KERNEL_SPINLOCK_H__

struct Cpu;

/**
 * Mutual exclusion spinlock.
 */
struct Spinlock {
  volatile int  locked;   ///< Whether the lock is held
  struct Cpu   *cpu;      ///< The CPU holding the lock
  const char   *name;     ///< For debugging purposes
  uintptr_t     pcs[10];  
};

#define SPIN_INITIALIZER(name)  { 0, NULL, (name), {} }

void spin_init(struct Spinlock *, const char *);
void spin_lock(struct Spinlock *);
void spin_unlock(struct Spinlock *);
int  spin_holding(struct Spinlock *);

#endif  // !__KERNEL_SPINLOCK_H__
