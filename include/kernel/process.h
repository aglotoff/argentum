#ifndef __KERNEL_PROCESS_H__
#define __KERNEL_PROCESS_H__

#ifndef __KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <sys/types.h>

#include <kernel/list.h>
#include <kernel/mm/mmu.h>
#include <kernel/trap.h>

struct File;
struct SpinLock;
struct Inode;

enum {
  PROCESS_EMBRIO   = 1,
  PROCESS_RUNNABLE = 2,
  PROCESS_RUNNING  = 3,
  PROCESS_SLEEPING = 4,
  PROCESS_ZOMBIE   = 5,
};

/**
 * Saved registers for kernel context swithces.
 */
struct Context {
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
 * Process state.
 */
struct Process {
  struct ListLink   link;       ///< Link into the containing list
  int               state;      ///< Process state

  pid_t             pid;        ///< Process identifier
  struct ListLink   pid_link;   ///< Link into the PID hash table

  struct Process   *parent;     ///< Link to the parent process
  struct ListLink   children;   ///< List pf process children
  struct ListLink   sibling;    ///< Link into the siblings list

  tte_t            *trtab;      ///< Translation table
  uintptr_t         heap;       ///< Process heap end
  uintptr_t         ustack;     ///< Process user stack bottom

  uint8_t          *kstack;     ///< Bottom of process kernel stack
  struct Trapframe *tf;         ///< Trap frame for current exception
  struct Context   *context;    ///< Saved context

  int               exit_code;

  struct File      *files[32];
  struct Inode     *cwd;
};

struct Process *my_process(void);

void            process_init(void);
int             process_create(const void *);
void            process_yield(void);
void            process_destroy(int);
void            process_free(struct Process *);
pid_t           process_copy(void);
pid_t           process_wait(pid_t, int *, int);

void            process_sleep(struct ListLink *, struct SpinLock *);
void            process_wakeup(struct ListLink *);
int             process_exec(const char *, char *const[], char *const[]);
void           *process_grow(ptrdiff_t);

void            scheduler(void);
void            context_switch(struct Context **old, struct Context *new);

extern int nprocesses;

#endif  // __KERNEL_PROCESS_H__
