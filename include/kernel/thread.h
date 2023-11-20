#ifndef __AG_INCLUDE_KERNEL_THREAD_H__
#define __AG_INCLUDE_KERNEL_THREAD_H__

#ifndef __AG_KERNEL__
#error "This is an Argentum kernel header; user programs should not include it"
#endif

struct Thread;

#include <sys/types.h>

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
struct Process;

struct Thread {
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
  /** CPU this thread is running on */
  struct Cpu       *cpu;

  /** Unique ID of this thread */
  int               id;
  /** Link into the ID hash chain */
  struct ListLink   id_link;
  /** The process this thread belongs to */
  struct Process   *process;
  /** Link into the process thread list */
  struct ListLink   process_link;

  /** Kernel stack */
  void             *kstack;
  /** Kernel stack size */
  size_t            kstack_size;
  /** Pointer to saved architecture-specific trap frame */
  void             *tf;
  /** Thread main function */
  void            (*func)(void *);
  /** The argument to be passed to the main function */
  void             *func_arg;
};

struct Thread *thread_current(void);
void           thread_init(void);
void           thread_start(void);
struct Thread *thread_create_user(struct Process *, uintptr_t);
void           thread_exit(int);
void           thread_tick(void);
void           thread_run(void);
void           thread_may_yield(void);

struct Process {
  /** Pointer to architecture-specific page table */
  void             *vm;
  /** Threads of this process */
  struct ListLink   threads;
  /** The number of currently active threads */
  int               active_threads;

  /** Unique ID of this process */
  pid_t             id;
  /** Link into the ID hash chain */
  struct ListLink   id_link;
};

void  process_init(void);
pid_t process_create(const void *);

struct Thread *thread_current(void);

#endif  // !__AG_INCLUDE_KERNEL_THREAD_H__
