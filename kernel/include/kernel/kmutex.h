#ifndef __KERNEL_INCLUDE_KERNEL_KMUTEX_H__
#define __KERNEL_INCLUDE_KERNEL_KMUTEX_H__

#ifndef __AG_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <stdint.h>

#include <kernel/list.h>
#include <kernel/spinlock.h>

struct Task;

/**
 * Mutex is a sleeping lock, i.e. when a task tries to acquire a mutex that
 * is locked, it is put to sleep until the mutex becomes available.
 *
 * Mutexes are used if the holding time is long or if the task needs to sleep
 * while holding the lock.
 */
struct KMutex {
  /** The task currently holding the mutex. */
  struct Task      *owner;
  /** List of tasks waiting for this mutex to be released. */
  struct ListLink   queue;
  /** Mutex name (for debugging purposes). */
  const char       *name;
};

int kmutex_init(struct KMutex *, const char *);
int kmutex_lock(struct KMutex *);
int kmutex_unlock(struct KMutex *);
int kmutex_holding(struct KMutex *);

#endif  // !__KERNEL_INCLUDE_KERNEL_KMUTEX_H__
