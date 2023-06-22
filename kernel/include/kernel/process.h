#ifndef __KERNEL_INCLUDE_KERNEL_PROCESS_H__
#define __KERNEL_INCLUDE_KERNEL_PROCESS_H__

#ifndef __AG_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <limits.h>
#include <sys/types.h>

#include <kernel/cpu.h>
#include <kernel/list.h>
#include <kernel/mm/vm.h>
#include <kernel/task.h>
#include <kernel/trap.h>
#include <kernel/wchan.h>

struct File;
struct SpinLock;
struct Inode;
struct Process;

struct ProcessThread {
  /** Kernel task associated with this process thread */
  struct Task           task;
  /** Tne process this thread belongs to */
  struct Process       *process;
  /** Unique thread identifier */
  pid_t                 pid;
  /** Link into the PID hash table */
  struct ListLink       pid_link;
  /** Bottom of the kernel-mode thread stack */
  uint8_t              *kstack;
  /** Address of the current trap frame on the stack */
  struct TrapFrame     *tf;
};

/**
 * Process descriptor.
 */
struct Process {
  /** The process' address space */
  struct VMSpace            *vm;

  /** Main process thread */
  struct ProcessThread *thread;
  
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
  struct File          *files[OPEN_MAX];
  /** Current working directory */
  struct Inode         *cwd;
};

static inline struct Process *
process_current(void)
{
  struct Task *task = task_current();
  return task != NULL ? ((struct ProcessThread *) task)->process : NULL;
}

void  process_init(void);
int   process_create(const void *, struct Process **);
void  process_destroy(int);
void  process_free(struct Process *);
pid_t process_copy(void);
pid_t process_wait(pid_t, int *, int);
int   process_exec(const char *, char *const[], char *const[]);
void *process_grow(ptrdiff_t);
void  process_thread_free(struct Task *);

#endif  // __KERNEL_INCLUDE_KERNEL_PROCESS_H__
