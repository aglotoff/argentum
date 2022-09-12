#ifndef __KERNEL_MUTEX_H__
#define __KERNEL_MUTEX_H__

#include <stdint.h>

#include <list.h>
#include <spin.h>

struct KThread;

struct Mutex {
  struct KThread   *thread;       ///< The thread holding the mutex
  struct ListLink   queue;        ///< Wait queue
  struct SpinLock   lock;         ///< Spinlock protecting this mutex
  const char       *name;         ///< The name of the mutex (for debugging)
};

void mutex_init(struct Mutex *, const char *);
void mutex_lock(struct Mutex *);
void mutex_unlock(struct Mutex *);
int  mutex_holding(struct Mutex *);

#endif  // !__KERNEL_MUTEX_H__
