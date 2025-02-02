#ifndef __KERNEL_INCLUDE_KERNEL_SIGNAL_H__
#define __KERNEL_INCLUDE_KERNEL_SIGNAL_H__

#include <signal.h>
#include <ucontext.h>

#include <kernel/core/list.h>

struct Process;

struct Signal {
  struct KListLink link;
  siginfo_t        info;
};

struct SignalFrame {
  uintptr_t  handler;
  siginfo_t  info;
  ucontext_t ucontext;
};

int  arch_signal_prepare(struct Process *, struct SignalFrame *);
int  arch_signal_return(struct Process *, struct SignalFrame *, int *);

void signal_init_system(void);
void signal_init(struct Process *);
int  signal_generate(pid_t, int, int);
void signal_clone(struct Process *, struct Process *);
void signal_deliver_pending(void);
int  signal_action_change(int, uintptr_t, struct sigaction *, struct sigaction *);
int  signal_return(void);
int  signal_pending(sigset_t *);
int  signal_mask_change(int, const sigset_t *, sigset_t *);
int  signal_suspend(const sigset_t *);
void signal_reset(struct Process *);

#endif // !__KERNEL_INCLUDE_KERNEL_SIGNAL_H__
