#ifndef __KERNEL_SYNC_H__
#define __KERNEL_SYNC_H__

#ifndef __KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

/**
 * @file kernel/sync.c
 * 
 * Kernel synchronization primitives.
 */

#include <stdint.h>

#include <kernel/list.h>

struct Cpu;
struct Process;

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

struct Mutex {
  struct Process   *process;      ///< The process holdng the mutex
  struct ListLink   queue;        ///< Wait queue
  struct SpinLock   lock;         ///< Spinlock protecting this mutex
  const char       *name;         ///< The name of the mutex (for debugging)
};

void mutex_init(struct Mutex *, const char *);
void mutex_lock(struct Mutex *);
void mutex_unlock(struct Mutex *);
int  mutex_holding(struct Mutex *);

#endif  // !__KERNEL_SYNC_H__
