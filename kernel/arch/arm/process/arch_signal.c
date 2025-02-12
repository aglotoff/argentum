#include <errno.h>

#include <kernel/process.h>
#include <kernel/signal.h>
#include <kernel/vmspace.h>
#include <kernel/console.h>

#include <arch/arm/regs.h>

int
arch_signal_prepare(struct Process *process, struct SignalFrame *frame)
{
  uintptr_t ctx_va = process->task->tf->sp - sizeof(struct SignalFrame);

  frame->ucontext.uc_mcontext.r0  = process->task->tf->r0;
  frame->ucontext.uc_mcontext.sp  = process->task->tf->sp;
  frame->ucontext.uc_mcontext.lr  = process->task->tf->lr;
  frame->ucontext.uc_mcontext.pc  = process->task->tf->pc;
  frame->ucontext.uc_mcontext.psr = process->task->tf->psr;

  if (vm_copy_out(process->vm->pgtab, frame, ctx_va, sizeof *frame) != 0)
    return SIGKILL;

  process->task->tf->r0 = ctx_va;
  process->task->tf->sp = ctx_va;
  process->task->tf->pc = process->signal_stub;

  return 0;
}

int
arch_signal_return(struct Process *process, struct SignalFrame *ctx, int *ret)
{
  int r;

  if ((r = vm_copy_in(process->vm->pgtab, ctx, process->task->tf->sp, sizeof *ctx) < 0))
    return r;

  // Prevent malicious users from executing in kernel mode
  if ((ctx->ucontext.uc_mcontext.psr & PSR_M_MASK) != PSR_M_USR)
    return -EINVAL;

  // No need to check other regs - bad values will lead to page faults

  process->task->tf->r0  = ctx->ucontext.uc_mcontext.r0;
  process->task->tf->sp  = ctx->ucontext.uc_mcontext.sp;
  process->task->tf->lr  = ctx->ucontext.uc_mcontext.lr;
  process->task->tf->pc  = ctx->ucontext.uc_mcontext.pc;
  process->task->tf->psr = ctx->ucontext.uc_mcontext.psr;

  *ret = process->task->tf->r0;

  return 0;
}
