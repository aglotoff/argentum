#ifndef __KERNEL_SPIN_H__
#define __KERNEL_SPIN_H__

#include <stdint.h>

struct Cpu;

#define NCALLERPCS  10

struct SpinLock {
  volatile int  locked;           ///< Whether the spinlock is held
  struct Cpu   *cpu;              ///< The CPU holding the spinlock
  const char   *name;             ///< The name of the spinlock (for debugging)
  uintptr_t     pcs[NCALLERPCS];  ///< Saved owner thread PCs (for debugging)
};

#define SPIN_INITIALIZER(name)  { 0, NULL, (name), {} }

void spin_init(struct SpinLock *, const char *);
void spin_lock(struct SpinLock *);
void spin_unlock(struct SpinLock *);
int  spin_holding(struct SpinLock *);

#endif  // !__KERNEL_SPIN_H__
