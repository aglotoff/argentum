#ifndef __KERNEL_INCLUDE_KERNEL_MUTEX_H__
#define __KERNEL_INCLUDE_KERNEL_MUTEX_H__

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <stdint.h>

#include <kernel/list.h>
#include <kernel/spinlock.h>

struct KThread;

/**
 * Mutex is a sleeping lock, i.e. when a task tries to acquire a mutex that
 * is locked, it is put to sleep until the mutex becomes available.
 *
 * Mutexes are used if the holding time is long or if the task needs to sleep
 * while holding the lock.
 */
struct KMutex {
  struct KListLink  link;
  struct KSpinLock  lock;
  /** The task currently holding the mutex. */
  struct KThread   *owner;
  /** List of tasks waiting for this mutex to be released. */
  struct KListLink  queue;
  /** Mutex name (for debugging purposes). */
  const char       *name;
  int               original_priority;
};

void           k_mutex_system_init(void);
void           k_mutex_init(struct KMutex *, const char *);
void           k_mutex_fini(struct KMutex *);
struct KMutex *k_mutex_create(const char *);
void           k_mutex_destroy(struct KMutex *);
int            k_mutex_lock(struct KMutex *);
int            k_mutex_unlock(struct KMutex *);
int            k_mutex_holding(struct KMutex *);

#endif  // !__KERNEL_INCLUDE_KERNEL_MUTEX_H__
