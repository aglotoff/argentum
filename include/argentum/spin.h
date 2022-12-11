#ifndef __INCLUDE_ARGENTUM_SPIN_H__
#define __INCLUDE_ARGENTUM_SPIN_H__

#ifndef __AG_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

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

#endif  // !__INCLUDE_ARGENTUM_SPIN_H__
