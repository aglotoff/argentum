#include <errno.h>

#include <kernel/process.h>
#include <kernel/signal.h>
#include <kernel/vmspace.h>
#include <kernel/console.h>

#include <arch/i386/mmu.h>
#include <arch/i386/regs.h>

int
arch_signal_prepare(struct Process *process, struct SignalFrame *frame)
{
  assert(process->thread != NULL);
  
  uintptr_t ctx_va = ROUND_DOWN(process->thread->task->tf->esp, 16) - sizeof(struct SignalFrame);

  frame->ucontext.uc_mcontext.eax    = process->thread->task->tf->eax;
  frame->ucontext.uc_mcontext.esp    = process->thread->task->tf->esp;
  frame->ucontext.uc_mcontext.ss     = process->thread->task->tf->ss;
  frame->ucontext.uc_mcontext.eip    = process->thread->task->tf->eip;
  frame->ucontext.uc_mcontext.cs     = process->thread->task->tf->cs;
  frame->ucontext.uc_mcontext.eflags = process->thread->task->tf->eflags;

  if (vm_copy_out(process->vm->pgtab, frame, ctx_va, sizeof *frame) != 0)
    return SIGKILL;

  process->thread->task->tf->esp = ctx_va;
  process->thread->task->tf->eax = ctx_va + offsetof(struct SignalFrame, ucontext) + offsetof(ucontext_t, uc_mcontext);
  process->thread->task->tf->eip = process->signal_stub;

  return 0;
}

int
arch_signal_return(struct Process *process, struct SignalFrame *ctx, int *ret)
{
  int r;

  assert(process->thread != NULL);
  
  if ((r = vm_copy_in(process->vm->pgtab, ctx, process->thread->task->tf->esp, sizeof *ctx) < 0))
    return r;

  if (ctx->ucontext.uc_mcontext.cs != SEG_USER_CODE)
    return -EINVAL;
  if (ctx->ucontext.uc_mcontext.ss != SEG_USER_DATA)
    return -EINVAL;
  if ((ctx->ucontext.uc_mcontext.eflags & EFLAGS_IOPL_MASK) != EFLAGS_IOPL_0)
    return -EINVAL;

  process->thread->task->tf->eax    = ctx->ucontext.uc_mcontext.eax;
  process->thread->task->tf->eip    = ctx->ucontext.uc_mcontext.eip;
  process->thread->task->tf->esp    = ctx->ucontext.uc_mcontext.esp;
  process->thread->task->tf->cs     = ctx->ucontext.uc_mcontext.cs;
  process->thread->task->tf->ss     = ctx->ucontext.uc_mcontext.ss;
  process->thread->task->tf->eflags = ctx->ucontext.uc_mcontext.eflags;

  *ret = process->thread->task->tf->eax;

  return 0;
}
