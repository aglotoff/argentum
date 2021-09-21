#ifndef KERNEL_SPINLOCK_H
#define KERNEL_SPINLOCK_H

struct Cpu;

/**
 * Mutual exclusion spinlock.
 */
struct Spinlock {
  volatile int  locked;   ///< Whether the lock is held
  struct Cpu   *cpu;      ///< The CPU holding the lock
  const char   *name;     ///< For debugging purposes
};

void spin_init(struct Spinlock *lock, const char *name);
int  spin_lock(struct Spinlock *lock);
void spin_unlock(struct Spinlock *lock, int flags);

int  irq_save(void);
void irq_restore(int flags);

#endif  // !KERNEL_SPINLOCK_H
