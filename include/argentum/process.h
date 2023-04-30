#ifndef __INCLUDE_ARGENTUM_PROCESS_H__
#define __INCLUDE_ARGENTUM_PROCESS_H__

#ifndef __AG_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <limits.h>
#include <sys/types.h>

#include <argentum/cpu.h>
#include <argentum/list.h>
#include <argentum/mm/vm.h>
#include <argentum/kthread.h>
#include <argentum/trap.h>
#include <argentum/wchan.h>

struct File;
struct SpinLock;
struct Inode;

struct ProcessThread {
  /** Kernel thread associated with this process thread */
  struct KThread        kernel_thread;
  /** Bottom of the kernel-mode thread stack */
  uint8_t              *kstack;
  /** Address of the current trap frame on the stack */
  struct TrapFrame     *tf;
};

/**
 * Process descriptor.
 */
struct Process {
  /** Unique process identifier */
  pid_t                 pid;
  /** Link into the PID hash table */
  struct ListLink       pid_link;

  /** The process' address space */
  struct VM            *vm;

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

  /** User ID */
  uid_t                 uid;
  /** Group ID */
  gid_t                 gid;
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
  struct KThread *thread = kthread_current();
  return thread != NULL ? thread->process : NULL;
}

void  process_init(void);
int   process_create(const void *, struct Process **);
void  process_destroy(int);
void  process_free(struct Process *);
pid_t process_copy(void);
pid_t process_wait(pid_t, int *, int);
int   process_exec(const char *, char *const[], char *const[]);
void *process_grow(ptrdiff_t);
void  process_thread_free(struct KThread *);

#endif  // __INCLUDE_ARGENTUM_PROCESS_H__
