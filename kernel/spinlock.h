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

void spin_init(struct Spinlock *lock, const char *name);
void spin_lock(struct Spinlock *lock);
void spin_unlock(struct Spinlock *lock);
int  spin_holding(struct Spinlock *lock);

void irq_save(void);
void irq_restore(void);

#endif  // !__KERNEL_SPINLOCK_H__
