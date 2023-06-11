#ifndef __KERNEL_INCLUDE_KERNEL_SEMAPHORE_H__
#define __KERNEL_INCLUDE_KERNEL_SEMAPHORE_H__

#include <kernel/list.h>

struct KSemaphore {
  unsigned long   count;
  struct ListLink queue;
};

int ksem_create(struct KSemaphore *, unsigned long);
int ksem_destroy(struct KSemaphore *);
int ksem_get(struct KSemaphore *, unsigned long);
int ksem_put(struct KSemaphore *);

#endif  // !__KERNEL_INCLUDE_KERNEL_SEMAPHORE_H__
