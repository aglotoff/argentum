#ifndef __KERNEL_INCLUDE_KERNEL_PROCESS_H__
#define __KERNEL_INCLUDE_KERNEL_PROCESS_H__

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <limits.h>
#include <signal.h>
#include <sys/times.h>
#include <sys/types.h>

#include <kernel/cpu.h>
#include <kernel/spinlock.h>
#include <kernel/list.h>
#include <kernel/vm.h>
#include <kernel/thread.h>
#include <kernel/trap.h>
#include <kernel/waitqueue.h>
#include <time.h>

struct File;
struct KSpinLock;
struct Inode;
struct Process;
struct PathNode;

struct FileDesc {
  struct File *file;
  int          flags;
};

/**
 * Process descriptor.
 */
struct Process {
  struct KListLink       link;
  /** The process' address space */
  struct VMSpace       *vm;

  /** Main process thread */
  struct KThread        *thread;

  /** Unique thread identifier */
  pid_t                 pid;
  /** Link into the PID hash table */
  struct KListLink       pid_link;
  /** Process group ID */
  pid_t                 pgid;

  /** The parent process */
  struct Process       *parent;
  /** List of child processes */
  struct KListLink      children;
  /** Link into the siblings list */
  struct KListLink      sibling_link;
  struct tms            times;

  /** Queue to sleep waiting for children */
  struct KWaitQueue     wait_queue;
  /** Whether the process is a zombie */
  int                   zombie;
  /** Exit code */
  int                   exit_code;

  uintptr_t             signal_stub;
  struct sigaction      signal_actions[NSIG];
  struct KListLink      signal_queue;
  sigset_t              signal_mask;
  sigset_t              signal_pending;

  /** Real user ID */
  uid_t                 ruid;
  /** Effective user ID */
  uid_t                 euid;
  /** Real group ID */
  gid_t                 rgid;
  /** Effective group ID */
  gid_t                 egid;
  /** File mode creation mask */
  mode_t                cmask;

  /** Current working directory */
  struct PathNode      *cwd;

  /** Open file descriptors */
  struct FileDesc       fd[OPEN_MAX];
  /** Lock protecting the file descriptors */
  struct KSpinLock      fd_lock;
};

extern struct KSpinLock __process_lock;
extern struct KListLink __process_list;

struct Signal {
  struct KListLink link;
  siginfo_t       info;
};

struct SignalFrame {
  // Saved by user:
  uint32_t s[32];
  uint32_t fpscr;
  uint32_t r1;
  uint32_t r2;
  uint32_t r3;
  uint32_t r4;
  uint32_t r5;
  uint32_t r6;
  uint32_t r7;
  uint32_t r8;
  uint32_t r9;
  uint32_t r10;
  uint32_t r11;
  uint32_t r12;

  // Saved by kernel:
  uint32_t signo;
  uint32_t handler;

  uint32_t r0;
  uint32_t sp;
  uint32_t lr;
  uint32_t pc;
  uint32_t psr;
  uint32_t trapno;
  sigset_t mask;
};

static inline struct Process *
process_current(void)
{
  struct KThread *task = k_thread_current();
  return task != NULL ? task->process : NULL;
}

static inline void
process_lock(void)
{
  k_spinlock_acquire(&__process_lock);
}

static inline void
process_unlock(void)
{
  k_spinlock_release(&__process_lock);
}

void           signal_init_system(void);
void           process_init(void);
int            process_create(const void *, struct Process **);
void           process_destroy(int);
void           process_free(struct Process *);
pid_t          process_copy(int);
pid_t          process_wait(pid_t, int *, int);
int            process_exec(const char *, char *const[], char *const[]);
void          *process_grow(ptrdiff_t);
void           arch_trap_frame_init(struct Process *, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
void           process_update_times(struct Process *, clock_t, clock_t);
void           process_get_times(struct Process *, struct tms *);

void           signal_init(struct Process *);
int            signal_generate(pid_t, int, int);
void           signal_clone(struct Process *, struct Process *);
void           signal_deliver_pending(void);
int            signal_action(int, uintptr_t, struct sigaction *, struct sigaction *);
int            signal_return(void);
int            signal_pending(sigset_t *);
int            signal_mask(int, const sigset_t *, sigset_t *);
int            signal_suspend(const sigset_t *);

int            process_nanosleep(const struct timespec *, struct timespec *);
pid_t          process_get_gid(pid_t);
int            process_set_gid(pid_t, pid_t);
int            process_match_pid(struct Process *, pid_t);

#endif  // __KERNEL_INCLUDE_KERNEL_PROCESS_H__
