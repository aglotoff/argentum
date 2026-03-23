#include <errno.h>

#include <kernel/process.h>
#include <kernel/signal.h>
#include <kernel/vmspace.h>
#include <kernel/console.h>

#include <arch/i386/mmu.h>
#include <arch/i386/regs.h>

int
arch_signal_prepare(struct Thread *thread, struct SignalFrame *frame)
{
  uintptr_t ctx_va = ROUND_DOWN(thread->tf->esp, 16) - sizeof(struct SignalFrame);

  k_assert(thread->process != NULL);

  frame->ucontext.uc_mcontext.eax    = thread->tf->eax;
  frame->ucontext.uc_mcontext.esp    = thread->tf->esp;
  frame->ucontext.uc_mcontext.ss     = thread->tf->ss;
  frame->ucontext.uc_mcontext.eip    = thread->tf->eip;
  frame->ucontext.uc_mcontext.cs     = thread->tf->cs;
  frame->ucontext.uc_mcontext.eflags = thread->tf->eflags;

  if (vm_space_copy_out(thread->process->vm, frame, ctx_va, sizeof *frame) != 0)
    return SIGKILL;

  thread->tf->esp = ctx_va;
  thread->tf->eax = ctx_va + offsetof(struct SignalFrame, ucontext) + offsetof(ucontext_t, uc_mcontext);
  thread->tf->eip = thread->process->signal_stub;

  return 0;
}

int
arch_signal_return(struct Thread *thread, struct SignalFrame *ctx, int *ret)
{
  int r;

  k_assert(thread->process != NULL);
  
  if ((r = vm_space_copy_in(thread->process->vm, ctx, thread->tf->esp, sizeof *ctx) < 0))
    return r;

  if (ctx->ucontext.uc_mcontext.cs != SEG_USER_CODE)
    return -EINVAL;
  if (ctx->ucontext.uc_mcontext.ss != SEG_USER_DATA)
    return -EINVAL;
  if ((ctx->ucontext.uc_mcontext.eflags & EFLAGS_IOPL_MASK) != EFLAGS_IOPL_0)
    return -EINVAL;

  thread->tf->eax    = ctx->ucontext.uc_mcontext.eax;
  thread->tf->eip    = ctx->ucontext.uc_mcontext.eip;
  thread->tf->esp    = ctx->ucontext.uc_mcontext.esp;
  thread->tf->cs     = ctx->ucontext.uc_mcontext.cs;
  thread->tf->ss     = ctx->ucontext.uc_mcontext.ss;
  thread->tf->eflags = ctx->ucontext.uc_mcontext.eflags;

  *ret = thread->tf->eax;

  return 0;
}
