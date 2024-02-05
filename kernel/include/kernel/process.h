#ifndef __KERNEL_INCLUDE_KERNEL_PROCESS_H__
#define __KERNEL_INCLUDE_KERNEL_PROCESS_H__

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <limits.h>
#include <signal.h>
#include <sys/types.h>

#include <kernel/cpu.h>
#include <kernel/list.h>
#include <kernel/mm/vm.h>
#include <kernel/thread.h>
#include <kernel/trap.h>
#include <kernel/wchan.h>
#include <time.h>

struct File;
struct SpinLock;
struct Inode;
struct Process;

struct FileDesc {
  struct File *file;
  int          flags;
};

/**
 * Process descriptor.
 */
struct Process {
  /** The process' address space */
  struct VMSpace       *vm;

  /** Main process thread */
  struct Thread        *thread;

  /** Unique thread identifier */
  pid_t                 pid;
  /** Link into the PID hash table */
  struct ListLink       pid_link;
  /** Process group ID */
  pid_t                 pgid;

  /** The parent process */
  struct Process       *parent;
  /** List of child processes */
  struct ListLink       children;
  /** Link into the siblings list */
  struct ListLink       sibling_link;

  /** Queue to sleep waiting for children */
  struct WaitChannel    wait_queue;
  /** Whether the process is a zombie */
  int                   zombie;
  /** Exit code */
  int                   exit_code;

  struct ListLink       pending_signals;
  uintptr_t             signal_stub;
  struct sigaction      signal_handlers[NSIG];

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
  /** Open file descriptors */
  struct FileDesc       fd[OPEN_MAX];
  /** Current working directory */
  struct Inode         *cwd;
};

struct Signal {
  struct ListLink link;
  siginfo_t       info;
};

static inline struct Process *
process_current(void)
{
  struct Thread *task = thread_current();
  return task != NULL ? task->process : NULL;
}

void           process_init(void);
int            process_create(const void *, struct Process **);
void           process_destroy(int);
void           process_free(struct Process *);
pid_t          process_copy(int);
pid_t          process_wait(pid_t, int *, int);
int            process_exec(const char *, char *const[], char *const[]);
void          *process_grow(ptrdiff_t);
void           arch_trap_frame_init(struct Process *, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
struct Signal *process_signal_create(int sig);
void           process_signal_send(struct Process *, struct Signal *);
void           process_signal_check(void);
int            process_signal_action(int, uintptr_t, struct sigaction *, struct sigaction *);
int            process_signal_return(void);
int            process_nanosleep(const struct timespec *, struct timespec *);
pid_t          process_get_gid(pid_t);
int            process_set_gid(pid_t, pid_t);

#endif  // __KERNEL_INCLUDE_KERNEL_PROCESS_H__
