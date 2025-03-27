#ifndef __KERNEL_INCLUDE_KERNEL_SEMAPHORE_H__
#define __KERNEL_INCLUDE_KERNEL_SEMAPHORE_H__

#include <kernel/core/config.h>
#include <kernel/core/list.h>
#include <kernel/core/spinlock.h>

struct KSemaphore {
  int              type;
  struct KSpinLock lock;
  unsigned long    count;
  struct KListLink queue;
};

#define K_SEMAPHORE_TYPE    0x53454D41  // {'S','E','M','A'}

void k_semaphore_create(struct KSemaphore *, int);
void k_semaphore_destroy(struct KSemaphore *);
int  k_semaphore_try_get(struct KSemaphore *);
int  k_semaphore_timed_get(struct KSemaphore *, k_tick_t, int);
int  k_semaphore_put(struct KSemaphore *);

static inline int
k_semaphore_get(struct KSemaphore *semaphore, int options)
{
  return k_semaphore_timed_get(semaphore, 0, options);
}

#endif  // !__KERNEL_INCLUDE_KERNEL_SEMAPHORE_H__
