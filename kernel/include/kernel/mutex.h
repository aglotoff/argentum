#ifndef __KERNEL_INCLUDE_KERNEL_MUTEX_H__
#define __KERNEL_INCLUDE_KERNEL_MUTEX_H__

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <stdint.h>

#include <kernel/core/list.h>
#include <kernel/spinlock.h>

struct KTask;

/**
 * Mutex is a sleeping lock, i.e. when a task tries to acquire a mutex that
 * is locked, it is put to sleep until the mutex becomes available.
 *
 * Mutexes are used if the holding time is long or if the task needs to sleep
 * while holding the lock.
 */
struct KMutex {
  int               type;
  int               flags;
  /** The task currently holding the mutex. */
  struct KTask   *owner;
  /** Link into the list of all mutexes owned by the same task. */
  struct KListLink  link;
  /** List of tasks waiting for this mutex to be released. */
  struct KListLink  queue;
  /** Priority of the highest task in the queue. */
  int               priority;
  /** Mutex name (for debugging purposes). */
  const char       *name;
};

#define K_MUTEX_TYPE    0x4D555458  // {'M','U','T','X'}
#define K_MUTEX_STATIC  (1 << 0)

void           k_mutex_system_init(void);
void           k_mutex_init(struct KMutex *, const char *);
void           k_mutex_fini(struct KMutex *);
struct KMutex *k_mutex_create(const char *);
void           k_mutex_destroy(struct KMutex *);
int            k_mutex_try_lock(struct KMutex *);
int            k_mutex_timed_lock(struct KMutex *, unsigned long);
int            k_mutex_unlock(struct KMutex *);
int            k_mutex_holding(struct KMutex *);

static inline int
k_mutex_lock(struct KMutex *mutex)
{
  return k_mutex_timed_lock(mutex, 0);
}

#endif  // !__KERNEL_INCLUDE_KERNEL_MUTEX_H__
