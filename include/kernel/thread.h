#ifndef __AG_INCLUDE_KERNEL_THREAD_H__
#define __AG_INCLUDE_KERNEL_THREAD_H__

#ifndef __AG_KERNEL__
#error "This is an Argentum kernel header; user programs should not include it"
#endif

struct Thread;

#include <kernel/list.h>
#include <arch/kernel/thread.h>

#define THREAD_PRIORITY_MAX 255

enum {
  /** The thread has not been initialized or has been destroyed */
  THREAD_STATE_NONE = 0,
  /** The tread is ready to run */
  THREAD_STATE_READY,
  /** The thread is currently running */
  THREAD_STATE_RUNNING,
  /** The thread has finished execution but requires cleanup */
  THREAD_STATE_DESTROYED,
};

enum {
  THREAD_FLAGS_YIELD = (1 << 0),
};

struct Cpu;

struct Thread {
  /** Unique ID of this thread */
  int               id;
  /** Link into the ID hash chain */
  struct ListLink   id_link;

  /** Link into the list containing this thread */
  struct ListLink   link;
  /** Current thread state */
  int               state;
  /** Thread flags */
  int               flags;
  /** Thread priority value */
  int               priority;
  /** Saved architecture-specific kernel context */
  void             *context;
  /** Kernel stack */
  void             *kstack;
  /** Thread main function */
  void            (*func)(void *);
  /** The argument to be passed to the main function */
  void             *func_arg;
  /** CPU this thread is running on */
  struct Cpu       *cpu;
};

void thread_init(void);
void thread_start(void);
int  thread_create(void (*)(void *), void *, int);
void thread_exit(int);
void thread_tick(void);
void thread_run(void);
void thread_may_yield(void);

#endif  // !__AG_INCLUDE_KERNEL_THREAD_H__
