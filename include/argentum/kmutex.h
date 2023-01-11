#ifndef __INCLUDE_ARGENTUM_KMUTEX_H__
#define __INCLUDE_ARGENTUM_KMUTEX_H__

#ifndef __AG_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <stdint.h>

#include <argentum/list.h>
#include <argentum/spinlock.h>

struct KThread;

struct KMutex {
  struct KThread   *thread;       ///< The thread holding the mutex
  struct ListLink   queue;        ///< Wait queue
  const char       *name;         ///< The name of the mutex (for debugging)
};

void kmutex_init(struct KMutex *, const char *);
void kmutex_lock(struct KMutex *);
void kmutex_unlock(struct KMutex *);
int  kmutex_holding(struct KMutex *);

#endif  // !__INCLUDE_ARGENTUM_KMUTEX_H__
