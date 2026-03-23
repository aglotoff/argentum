#include <errno.h>

#include <kernel/process.h>
#include <kernel/signal.h>
#include <kernel/vmspace.h>
#include <kernel/console.h>

#include <arch/arm/regs.h>

int
arch_signal_prepare(struct Thread *thread, struct SignalFrame *frame)
{
  uintptr_t ctx_va = thread->tf->sp - sizeof(struct SignalFrame);

  k_assert(thread->process != NULL);

  frame->ucontext.uc_mcontext.r0  = thread->tf->r0;
  frame->ucontext.uc_mcontext.sp  = thread->tf->sp;
  frame->ucontext.uc_mcontext.lr  = thread->tf->lr;
  frame->ucontext.uc_mcontext.pc  = thread->tf->pc;
  frame->ucontext.uc_mcontext.psr = thread->tf->psr;

  if (vm_space_copy_out(thread->process->vm, frame, ctx_va, sizeof *frame) != 0)
    return SIGKILL;

  thread->tf->r0 = ctx_va;
  thread->tf->sp = ctx_va;
  thread->tf->pc = thread->process->signal_stub;

  return 0;
}

int
arch_signal_return(struct Thread *thread, struct SignalFrame *ctx, int *ret)
{
  int r;

  k_assert(thread->process != NULL);

  if ((r = vm_space_copy_in(thread->process->vm, ctx, thread->tf->sp, sizeof *ctx) < 0))
    return r;

  // Prevent malicious users from executing in kernel mode
  if ((ctx->ucontext.uc_mcontext.psr & PSR_M_MASK) != PSR_M_USR)
    return -EINVAL;

  // No need to check other regs - bad values will lead to page faults

  thread->tf->r0  = ctx->ucontext.uc_mcontext.r0;
  thread->tf->sp  = ctx->ucontext.uc_mcontext.sp;
  thread->tf->lr  = ctx->ucontext.uc_mcontext.lr;
  thread->tf->pc  = ctx->ucontext.uc_mcontext.pc;
  thread->tf->psr = ctx->ucontext.uc_mcontext.psr;

  *ret = thread->tf->r0;

  return 0;
}
