#ifndef __KERNEL_SCHEDULER_H__
#define __KERNEL_SCHEDULER_H__

#ifndef __KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <stdint.h>

#include <kernel/list.h>

struct Process;
struct SpinLock;

enum {
  TASK_RUNNABLE     = 1,
  TASK_RUNNING      = 2,
  TASK_NOT_RUNNABLE = 3,
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

/**
 * task state.
 */
struct Task {
  struct ListLink   link;           ///< Link into the containing list
  int               state;          ///< task state
  struct Context   *context;        ///< Saved context
  void            (*entry)(void);   ///< task entry point
  struct Process   *process;        ///< The process this task belongs to
};

void         scheduler_init(void);
void         scheduler_start(void);

struct Task *task_create(struct Process *, void (*)(void), uint8_t *);
void         task_destroy(struct Task *);
void         task_enqueue(struct Task *);
void         task_run(void);
void         task_yield(void);
void         task_sleep(struct ListLink *, struct SpinLock *);
void         task_wakeup(struct ListLink *);

#endif  // __KERNEL_SCHEDULER_H__
