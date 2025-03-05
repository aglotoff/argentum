#ifndef __KERNEL_INCLUDE_KERNEL_SEMAPHORE_H__
#define __KERNEL_INCLUDE_KERNEL_SEMAPHORE_H__

#include <kernel/core/list.h>
#include <kernel/core/spinlock.h>

struct KSemaphore {
  int              type;
  int              flags;
  struct KSpinLock lock;
  unsigned long    count;
  struct KListLink queue;
};

#define K_SEMAPHORE_TYPE    0x53454D41  // {'S','E','M','A'}
#define K_SEMAPHORE_STATIC  (1 << 0)

void               k_semaphore_system_init(void);
void               k_semaphore_init(struct KSemaphore *, int);
void               k_semaphore_fini(struct KSemaphore *);
struct KSemaphore *k_semaphore_create(int);
void               k_semaphore_destroy(struct KSemaphore *);
int                k_semaphore_try_get(struct KSemaphore *);
int                k_semaphore_timed_get(struct KSemaphore *, unsigned long);
int                k_semaphore_put(struct KSemaphore *);

static inline int
k_semaphore_get(struct KSemaphore *semaphore)
{
  return k_semaphore_timed_get(semaphore, 0);
}

#endif  // !__KERNEL_INCLUDE_KERNEL_SEMAPHORE_H__
