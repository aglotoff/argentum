#ifndef __INCLUDE_ARGENTUM_MUTEX_H__
#define __INCLUDE_ARGENTUM_MUTEX_H__

#ifndef __AG_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <stdint.h>

#include <argentum/list.h>
#include <argentum/spin.h>

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

#endif  // !__INCLUDE_ARGENTUM_MUTEX_H__
