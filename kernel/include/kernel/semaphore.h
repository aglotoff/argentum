#ifndef __KERNEL_INCLUDE_KERNEL_SEMAPHORE_H__
#define __KERNEL_INCLUDE_KERNEL_SEMAPHORE_H__

#include <kernel/list.h>
#include <kernel/spinlock.h>

struct KSemaphore {
  struct KSpinLock lock;
  unsigned long    count;
  struct KListLink queue;
  int              flags;
};

#define KSEMAPHORE_STATIC   1

void               k_semaphore_system_init(void);
void               k_semaphore_init(struct KSemaphore *, int);
void               k_semaphore_fini(struct KSemaphore *);
struct KSemaphore *k_semaphore_create(int);
void               k_semaphore_destroy(struct KSemaphore *);
int                k_semaphore_timed_get(struct KSemaphore *, unsigned long);
int                k_semaphore_put(struct KSemaphore *);

static inline int
k_semaphore_get(struct KSemaphore *sem)
{
  return k_semaphore_timed_get(sem, 0);
}

#endif  // !__KERNEL_INCLUDE_KERNEL_SEMAPHORE_H__
