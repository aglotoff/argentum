#ifndef __KERNEL_INCLUDE_KERNEL_THREAD_H__
#define __KERNEL_INCLUDE_KERNEL_THREAD_H__

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <limits.h>
#include <stdint.h>

#include <kernel/core/tick.h>
#include <arch/context.h>

#define THREAD_MAX_PRIORITIES  (2 * NZERO)

enum {
  THREAD_STATE_NONE = 0,
  THREAD_STATE_READY,
  THREAD_STATE_RUNNING,
  THREAD_STATE_SLEEP,
  THREAD_STATE_MUTEX,
  THREAD_STATE_SUSPENDED,
  THREAD_STATE_DESTROYED,
};

enum {
  THREAD_FLAG_RESCHEDULE = (1 << 0),
  THREAD_FLAG_DESTROY    = (1 << 1),
};

struct KObjectPool;
extern struct KObjectPool *thread_cache;

struct Process;
struct KMutex;

/**
 * Scheduler task state.
 */
struct KThread {
  char              type[4];
  /** Link into the list containing this task */
  struct KListLink  link;
  /** Current task state */
  int               state;
  /** Task priority value */
  int               priority;
  int               saved_priority;
  /** Various flags */
  int               flags;
  /** CPU */
  struct KCpu       *cpu;

  struct KListLink   owned_mutexes;
  struct KMutex     *sleep_on_mutex;

  /** Bottom of the kernel-mode stack */
  void              *kstack;
  /** Address of the current trap frame on the stack */
  struct TrapFrame  *tf;
  /** Saved kernel context */
  struct Context    *context;

  /** Entry point function */
  void            (*entry)(void *);
  /** The argument for the entry function*/
  void             *arg;

  /** Timer for timeouts */
  struct KTimeout timer;
  /** Value that indicated sleep result */
  int               sleep_result;
  int               err;

  /** Tne process this thread belongs to */
  struct Process   *process;
  int               stat;
};

void            arch_thread_init_stack(struct KThread *, void (*)(void));
void            arch_thread_idle(void);

struct KThread *k_thread_current(void);
struct KThread *k_thread_create(struct Process *, void (*)(void *), void *, int);
void            k_thread_exit(void);
int             k_thread_resume(struct KThread *, int);
void            k_thread_suspend(void);
void            k_thread_yield(void);
void            thread_cleanup(struct KThread *);
void            k_thread_interrupt(struct KThread *);

void            k_sched_init(void);
void            k_sched_start(void);

#endif  // __KERNEL_INCLUDE_KERNEL_THREAD_H__
