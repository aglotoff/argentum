#ifndef __KERNEL_THREAD_H__
#define __KERNEL_THREAD_H__

#ifndef __KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <stdint.h>

#include <kernel/list.h>

struct Process;
struct SpinLock;

enum {
  THREAD_EMBRIO    = 1,
  THREAD_RUNNABLE  = 2,
  THREAD_RUNNING   = 3,
  THREAD_SLEEPING  = 4,
  THREAD_DESTROYED = 5,
};

/**
 * Saved registers for kernel context swithces.
 */
struct Context {
  uint32_t fpscr;
  uint32_t d[32];
  uint32_t r4;
  uint32_t r5;
  uint32_t r6;
  uint32_t r7;
  uint32_t r8;
  uint32_t r9;
  uint32_t r10;
  uint32_t r11;
  uint32_t lr;
};

/**
 * Thread state.
 */
struct Thread {
  struct ListLink   link;           ///< Link into the containing list
  int               state;          ///< Thread state
  struct Context   *context;        ///< Saved context
  void            (*entry)(void);   ///< Thread entry point
  struct Process   *process;        ///< The process this thread belongs to
};

struct Thread *my_thread(void);

void           scheduler_init(void);
void           scheduler_start(void);

void           thread_init(struct Thread *, void (*)(void), uint8_t *,
                           struct Process *);
void           thread_enqueue(struct Thread *);
void           thread_run(void);
void           thread_yield(void);
void           thread_sleep(struct ListLink *, struct SpinLock *, int);
void           thread_wakeup(struct ListLink *);

#endif  // __KERNEL_THREAD_H__
