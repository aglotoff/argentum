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
  uintptr_t ctx_va = ROUND_DOWN(process->thread->tf->esp, 16) - sizeof(struct SignalFrame);

  frame->ucontext.uc_mcontext.eax    = process->thread->tf->eax;
  frame->ucontext.uc_mcontext.esp    = process->thread->tf->esp;
  frame->ucontext.uc_mcontext.ss     = process->thread->tf->ss;
  frame->ucontext.uc_mcontext.eip    = process->thread->tf->eip;
  frame->ucontext.uc_mcontext.cs     = process->thread->tf->cs;
  frame->ucontext.uc_mcontext.eflags = process->thread->tf->eflags;

  if (vm_copy_out(process->vm->pgtab, frame, ctx_va, sizeof *frame) != 0)
    return SIGKILL;

  process->thread->tf->esp = ctx_va;
  process->thread->tf->eax = ctx_va + offsetof(struct SignalFrame, ucontext) + offsetof(ucontext_t, uc_mcontext);
  process->thread->tf->eip = process->signal_stub;

  return 0;
}

int
arch_signal_return(struct Process *process, struct SignalFrame *ctx, int *ret)
{
  int r;
  
  if ((r = vm_copy_in(process->vm->pgtab, ctx, process->thread->tf->esp, sizeof *ctx) < 0))
    return r;

  if (ctx->ucontext.uc_mcontext.cs != SEG_USER_CODE)
    return -EINVAL;
  if (ctx->ucontext.uc_mcontext.ss != SEG_USER_DATA)
    return -EINVAL;
  if ((ctx->ucontext.uc_mcontext.eflags & EFLAGS_IOPL_MASK) != EFLAGS_IOPL_0)
    return -EINVAL;

  process->thread->tf->eax    = ctx->ucontext.uc_mcontext.eax;
  process->thread->tf->eip    = ctx->ucontext.uc_mcontext.eip;
  process->thread->tf->esp    = ctx->ucontext.uc_mcontext.esp;
  process->thread->tf->cs     = ctx->ucontext.uc_mcontext.cs;
  process->thread->tf->ss     = ctx->ucontext.uc_mcontext.ss;
  process->thread->tf->eflags = ctx->ucontext.uc_mcontext.eflags;

  *ret = process->thread->tf->eax;

  //cprintf("restoring ret value of %d\n", *ret);

  return 0;
}
