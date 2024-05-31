#ifndef __KERNEL_INCLUDE_KERNEL_semaphoreAPHORE_H__
#define __KERNEL_INCLUDE_KERNEL_semaphoreAPHORE_H__

#include <kernel/list.h>

struct KSemaphore {
  unsigned long   count;
  struct ListLink queue;
};

int k_semaphore_create(struct KSemaphore *, unsigned long);
int k_semaphore_destroy(struct KSemaphore *);
int k_semaphore_get(struct KSemaphore *, unsigned long, int);
int k_semaphore_put(struct KSemaphore *);

#endif  // !__KERNEL_INCLUDE_KERNEL_semaphoreAPHORE_H__
