#ifndef __INCLUDE_ARGENTUM_KMUTEX_H__
#define __INCLUDE_ARGENTUM_KMUTEX_H__

#ifndef __AG_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <stdint.h>

#include <argentum/list.h>
#include <argentum/spinlock.h>

struct KThread;

/**
 * Mutex is a sleeping lock, i.e. when a thread tries to acquire a mutex that
 * is locked, it is put to sleep until the mutex becomes available.
 *
 * Mutexes are used if the holding time is long or if the thread needs to sleep
 * while holding the lock.
 */
struct KMutex {
  /** The thread currently holding the mutex. */
  struct KThread   *owner;
  /** List of threads waiting for this mutex to be released. */
  struct ListLink   queue;
  /** Spinlock protecting the mutex. */
  struct SpinLock   lock;
  /** Mutex name (for debugging purposes). */
  const char       *name;
};

void kmutex_init(struct KMutex *, const char *);
void kmutex_lock(struct KMutex *);
void kmutex_unlock(struct KMutex *);
int  kmutex_holding(struct KMutex *);

#endif  // !__INCLUDE_ARGENTUM_KMUTEX_H__
