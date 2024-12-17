#ifndef __KERNEL_INCLUDE_KERNEL_WAITQUEUE_H__
#define __KERNEL_INCLUDE_KERNEL_WAITQUEUE_H__

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <kernel/core/list.h>

struct KSpinLock;

/**
 * Wait channel is a structure that allows tasks in the kernel to wait for
 * some associated resource. 
 */
struct KWaitQueue {
  /** List of tasks waiting on the channel. */
  struct KListLink head;
};

/**
 * Initialize a static wait channel.
 */
#define k_waitqueue_initIALIZER { .head = KLIST_INITIALIZER }

void k_waitqueue_init(struct KWaitQueue *);
int  k_waitqueue_sleep(struct KWaitQueue *, struct KSpinLock *);
int  k_waitqueue_timed_sleep(struct KWaitQueue *, struct KSpinLock *, unsigned long);
void k_waitqueue_wakeup_one(struct KWaitQueue *);
void k_waitqueue_wakeup_all(struct KWaitQueue *);

#endif  // !__KERNEL_INCLUDE_KERNEL_WAITQUEUE_H__
