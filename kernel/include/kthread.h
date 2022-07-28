#ifndef __KERNEL_KTHREAD_H__
#define __KERNEL_KTHREAD_H__

#include <stdint.h>

#include <list.h>

struct Process;
struct SpinLock;

enum {
  KTHREAD_RUNNABLE     = 1,
  KTHREAD_RUNNING      = 2,
  KTHREAD_NOT_RUNNABLE = 3,
  KTHREAD_DESTROYED    = 4,
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
 * thread state.
 */
struct KThread {
  struct ListLink   link;           ///< Link into the containing list
  int               state;          ///< Thread state
  uint8_t          *kstack;         ///< Bottom of the kernel-mode stack
  struct Context   *context;        ///< Saved context
  void            (*entry)(void);   ///< Thread entry point
  struct Process   *process;        ///< The process this thread belongs to
};

struct KThread *kthread_create(struct Process *, void (*)(void), uint8_t *);
void            kthread_destroy(struct KThread *);
void            kthread_enqueue(struct KThread *);
void            kthread_run(void);
void            kthread_yield(void);
void            kthread_sleep(struct ListLink *, struct SpinLock *);
void            kthread_wakeup(struct ListLink *);

void            scheduler_init(void);
void            scheduler_start(void);

#endif  // __KERNEL_KTHREAD_H__
