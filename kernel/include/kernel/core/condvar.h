#ifndef __KERNEL_INCLUDE_KERNEL_CONDVAR_H__
#define __KERNEL_INCLUDE_KERNEL_CONDVAR_H__

#include <kernel/core/list.h>
#include <kernel/core/spinlock.h>

struct KCondVar {
  int              type;
  int              flags;
  struct KListLink queue;
};

#define K_CONDVAR_TYPE    0x434F4E44  // {'C','O','N','D'}

struct KMutex;

void             k_condvar_create(struct KCondVar *);
void             k_condvar_destroy(struct KCondVar *);
int              k_condvar_timed_wait(struct KCondVar *, struct KMutex *, unsigned long);
int              k_condvar_signal(struct KCondVar *);
int              k_condvar_broadcast(struct KCondVar *);

static inline int
k_condvar_wait(struct KCondVar *cond, struct KMutex *mutex)
{
  return k_condvar_timed_wait(cond, mutex, 0);
}

#endif  // !__KERNEL_INCLUDE_KERNEL_CONDVAR_H__
