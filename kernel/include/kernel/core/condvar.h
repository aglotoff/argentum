#ifndef __KERNEL_INCLUDE_KERNEL_CONDVAR_H__
#define __KERNEL_INCLUDE_KERNEL_CONDVAR_H__

#include <kernel/core/list.h>
#include <kernel/spinlock.h>

struct KCondVar {
  int              type;
  int              flags;
  struct KListLink queue;
};

#define K_CONDVAR_TYPE    0x434F4E44  // {'C','O','N','D'}
#define K_CONDVAR_STATIC  (1 << 0)

struct KMutex;

void             k_condvar_system_init(void);
void             k_condvar_init(struct KCondVar *);
void             k_condvar_fini(struct KCondVar *);
struct KCondVar *k_condvar_create(void);
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
