#ifndef __KERNEL_PROCESS_H__
#define __KERNEL_PROCESS_H__

#ifndef __KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <limits.h>
#include <sys/types.h>

#include <kernel/list.h>
#include <kernel/mm/vm.h>
#include <kernel/thread.h>
#include <kernel/trap.h>

struct File;
struct SpinLock;
struct Inode;

/**
 * Process state.
 */
struct Process {
  struct Thread     *thread;

  uint8_t           *kstack;      ///< Bottom of thread kernel stack
  struct UTrapframe *tf;          ///< Trap frame for current exception

  struct UserVm      vm;

  pid_t              pid;         ///< Process identifier
  struct ListLink    pid_link;    ///< Link into the PID hash table

  // Protected by process_lock:
  struct Process    *parent;      ///< Link to the parent process
  struct ListLink    wait_queue;  ///< Queue to sleep waiting for children
  struct ListLink    children;    ///< List pf process children
  struct ListLink    sibling;     ///< Link into the siblings list

  int                exit_code;

  struct File       *files[OPEN_MAX];
  struct Inode      *cwd;
};

static inline struct Process *
my_process(void)
{
  struct Thread *thread = my_thread();
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

#endif  // __KERNEL_PROCESS_H__
