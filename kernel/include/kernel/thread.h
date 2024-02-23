#ifndef __KERNEL_INCLUDE_KERNEL_THREAD_H__
#define __KERNEL_INCLUDE_KERNEL_THREAD_H__

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <limits.h>
#include <stdint.h>

#include <kernel/ktimer.h>
#include <kernel/list.h>
#include <kernel/spin.h>

#define THREAD_MAX_PRIORITIES  (2 * NZERO)

enum {
  THREAD_STATE_NONE = 0,
  THREAD_STATE_READY,
  THREAD_STATE_RUNNING,
  THREAD_STATE_SLEEPING,
  THREAD_STATE_SUSPENDED,
  THREAD_STATE_DESTROYED,
};

enum {
  THREAD_FLAG_RESCHEDULE = (1 << 0),
  THREAD_FLAG_DESTROY    = (1 << 1),
};

/**
 * Saved registers for kernel context switches (SP is saved implicitly).
 * See https://wiki.osdev.org/Calling_Conventions
 */
struct Context {
  uint32_t s[32];
  uint32_t fpscr;
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

struct ObjectPool;
extern struct ObjectPool *thread_cache;

struct Process;

/**
 * Scheduler task state.
 */
struct Thread {
  /** Link into the list containing this task */
  struct ListLink   link;
  /** Current task state */
  int               state;
  /** Task priority value */
  int               priority;
  /** Various flags */
  int               flags;
  /** CPU */
  struct Cpu       *cpu;

  /** Bottom of the kernel-mode stack */
  void             *kstack;
  /** Address of the current trap frame on the stack */
  struct TrapFrame *tf;
  /** Saved kernel context */
  struct Context   *context;

  /** Entry point function */
  void            (*entry)(void *);
  /** The argument for the entry function*/
  void             *arg;

  /** Timer for timeouts */
  struct KTimer     timer;
  /** Value that indicated sleep result */
  int               sleep_result;
  int               err;

  /** Tne process this thread belongs to */
  struct Process   *process;
};

struct Thread *thread_current(void);
struct Thread *thread_create(struct Process *, void (*)(void *), void *, int);
void         thread_exit(void);
int          thread_resume(struct Thread *);
void         thread_yield(void);
int          sched_sleep(struct ListLink *, unsigned long, struct SpinLock *);
void         thread_cleanup(struct Thread *);

void         sched_may_yield(struct Thread *);
void         sched_yield(void);
void         sched_enqueue(struct Thread *);
void         sched_wakeup_all(struct ListLink *, int);
void         sched_wakeup_one(struct ListLink *, int);
void         sched_init(void);
void         sched_start(void);
void         ktime_tick(void);
void         sched_isr_enter(void);
void         sched_isr_exit(void);

extern struct SpinLock __sched_lock;

static inline void
sched_lock(void)
{
  spin_lock(&__sched_lock);
}

static inline void
sched_unlock(void)
{
  spin_unlock(&__sched_lock);
}

#endif  // __KERNEL_INCLUDE_KERNEL_THREAD_H__
