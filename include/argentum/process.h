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

/**
 * Process descriptor.
 */
struct Process {
  struct KThread    *thread;          ///< Thread associated with this process
  uint8_t           *kstack;          ///< Bottom of the kernel-mode stack
  struct TrapFrame  *tf;              ///< Current trap frame

  pid_t              pid;             ///< Process identifier
  struct ListLink    pid_link;        ///< Link into the PID hash table

  struct VM         *vm;              ///< Process' address space

  struct Process    *parent;          ///< Link to the parent process
  struct WaitChannel   wait_queue;      ///< Queue to sleep waiting for children
  struct ListLink    children;        ///< List of child processes
  struct ListLink    sibling;         ///< Link into the siblings list
  int                zombie;          ///< Whether the process is a zombie
  int                exit_code;       ///< Exit code

  uid_t              uid;             ///< User ID
  gid_t              gid;             ///< Group ID
  mode_t             cmask;           ///< File mode creation mask
  struct File       *files[OPEN_MAX]; ///< Open file descriptors
  struct Inode      *cwd;             ///< Current working directory
};

static inline struct Process *
my_process(void)
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

#endif  // __INCLUDE_ARGENTUM_PROCESS_H__
