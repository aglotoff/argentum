#ifndef __INCLUDE_ARGENTUM_WAITQUEUE_H__
#define __INCLUDE_ARGENTUM_WAITQUEUE_H__

#ifndef __AG_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <argentum/list.h>

struct SpinLock;

struct WaitQueue {
  struct ListLink head;
};

void waitqueue_init(struct WaitQueue *);
void waitqueue_sleep(struct WaitQueue *, struct SpinLock *);
void waitqueue_wakeup(struct WaitQueue *);
void waitqueue_wakeup_all(struct WaitQueue *);

#endif  // !__INCLUDE_ARGENTUM_WAITQUEUE_H__
