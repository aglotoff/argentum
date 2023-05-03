#ifndef __INCLUDE_ARGENTUM_WCHAN_H__
#define __INCLUDE_ARGENTUM_WCHAN_H__

#ifndef __AG_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <argentum/list.h>

struct SpinLock;

/**
 * Wait channel is a structure that allows tasks in the kernel to wait for
 * some associated resource. 
 */
struct WaitChannel {
  /** List of tasks waiting on the channel. */
  struct ListLink head;
};

/**
 * Initialize a static wait channel.
 */
#define WCHAN_INITIALIZER { .head = LIST_INITIALIZER }

void wchan_init(struct WaitChannel *);
void wchan_sleep(struct WaitChannel *, struct SpinLock *);
void wchan_wakeup_one(struct WaitChannel *);
void wchan_wakeup_all(struct WaitChannel *);

#endif  // !__INCLUDE_ARGENTUM_WCHAN_H__
