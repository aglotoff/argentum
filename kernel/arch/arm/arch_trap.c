#include <signal.h>

#include <kernel/console.h>
#include <kernel/page.h>
#include <kernel/vmspace.h>
#include <kernel/process.h>
#include <kernel/sys.h>
#include <kernel/trap.h>
#include <kernel/types.h>
#include <kernel/core/irq.h>
#include <kernel/monitor.h>
#include <kernel/core/semaphore.h>
#include <arch/arm/mach.h>
#include <kernel/interrupt.h>
#include <kernel/time.h>
#include <kernel/signal.h>

#include <arch/arm/regs.h>

static void trap_handle_abort(struct TrapFrame *);

/**
 * Common entry point for all traps, including system calls. The TrapFrame
 * structure is built on the stack in trapentry.S
 */
void
trap(struct TrapFrame *tf)
{
  extern char *panicstr;

  struct Process *my_process = process_current();

  // Halt if some other CPU already has called k_panic().
  if (panicstr) {
    for (;;) {
      asm volatile("wfi");
    }
  }

  // Dispatch based on what type of trap occured.
  switch (tf->trapno) {
  case T_DABT:
  case T_PABT:
    trap_handle_abort(tf);
    break;
  case T_SWI:
    k_irq_enable();
    tf->r0 = sys_dispatch();
    k_irq_disable();
    break;
  case T_IRQ:
    interrupt_dispatch(tf);
    break;
  case T_UNDEF:
    if ((tf->psr & PSR_M_MASK) == PSR_M_USR) {
      // TODO: ILL_ILLOPC
      if (signal_generate(my_process->pid, SIGILL, 0) != 0)
        k_panic("sending SIGILL failed");
      break;
    }
    // fall through
  default:
    print_trapframe(tf);
    k_panic("unhandled trap in kernel");
  }

  if ((tf->psr & PSR_M_MASK) == PSR_M_USR) {
    signal_deliver_pending();

    while (my_process->state != PROCESS_STATE_ACTIVE) {
      k_task_suspend();
      signal_deliver_pending();
    }
  }
}

// Handle data or prefetch abort
static void
trap_handle_abort(struct TrapFrame *tf)
{
  uint32_t address, status;
  struct Process *process;

  // Read the contents of the corresponsing Fault Address Register (FAR) and 
  // the Fault Status Register (FSR).
  address = tf->trapno == T_DABT ? cp15_dfar_get() : cp15_ifar_get();
  status  = tf->trapno == T_DABT ? cp15_dfsr_get() : cp15_ifsr_get();

  // If abort happened in kernel mode, print the trap frame and panic
  if ((tf->psr & PSR_M_MASK) != PSR_M_USR) {
    print_trapframe(tf);
    k_panic("kernel fault va %p status %#x", address, status);
  }

  process = process_current();
  k_assert(process != NULL);

  // Try to handle VM fault first (it may be caused by copy-on-write pages)
  if (((status & 0xF) == 0xF) && vm_handle_fault(process->vm->pgtab, address) == 0) {
    return;
  }

  // If unsuccessfull, kill the process
  print_trapframe(tf);
  k_panic("[%d %s]: user fault va %p status %#x\n", process->pid, process->name, address, status);

  // TODO: SEGV_MAPERR or SEGV_ACCERR
  if (signal_generate(process->pid, SIGSEGV, 0) != 0)
    k_panic("sending SIGSEGV failed");
}

// Returns a human-readable name for the given trap number
static const char *
get_trap_name(unsigned trapno)
{
  static const char *const known_traps[] = {
    "Reset",
    "Undefined Instruction",
    "Supervisor Call",
    "Prefetch Abort",
    "Data Abort",
    "Not used",
    "IRQ",
    "FIQ",
  };

  if (trapno <= T_FIQ)
    return known_traps[trapno];
  return "(unknown trap)";
}

/**
 * Display the contents of the given trap frame in a nice format.
 * 
 * @param tf Base address of the trap frame structure to be printed.
 */
void
print_trapframe(struct TrapFrame *tf)
{
  static const char *const modes[32] = {
    [PSR_M_USR] = "USR",
    [PSR_M_FIQ] = "FIQ",
    [PSR_M_IRQ] = "IRQ",
    [PSR_M_SVC] = "SVC",
    [PSR_M_MON] = "MON",
    [PSR_M_ABT] = "ABT",
    [PSR_M_UND] = "UND",
    [PSR_M_SYS] = "SYS",
  };

  cprintf("TRAP frame at %p from CPU %d\n", tf, k_cpu_id());
  cprintf("  psr  %p    [%s%s%s%s]\n",
          tf->psr,
          tf->psr & PSR_I ? "I," : "",
          tf->psr & PSR_I ? "F," : "",
          tf->psr & PSR_I ? "T," : "",
          modes[tf->psr & PSR_M_MASK] ? modes[tf->psr & PSR_M_MASK] : "");
  cprintf("  trap %p    [%s]\n", tf->trapno, get_trap_name(tf->trapno));
  cprintf("  sp   %p    lr   %p\n", tf->sp,  tf->lr);
  cprintf("  r0   %p    r1   %p\n", tf->r0,  tf->r1);
  cprintf("  r2   %p    r3   %p\n", tf->r2,  tf->r3);
  cprintf("  r4   %p    r5   %p\n", tf->r4,  tf->r5);
  cprintf("  r6   %p    r7   %p\n", tf->r6,  tf->r7);
  cprintf("  r8   %p    r9   %p\n", tf->r8,  tf->r9);
  cprintf("  r10  %p    r11  %p\n", tf->r10, tf->r11);
  cprintf("  r12  %p    pc   %p\n", tf->r12, tf->pc);
}

int
ipi_irq(int, void *)
{
  return 1;
}

int 
arch_trap_frame_init(struct Process *process, uintptr_t entry, uintptr_t arg1,
                     uintptr_t arg2, uintptr_t arg3, uintptr_t sp)
{
  process->thread->tf->r0  = arg1;              // argc
  process->thread->tf->r1  = arg2;              // argv
  process->thread->tf->r2  = arg3;              // environ
  process->thread->tf->sp  = sp;                // stack pointer
  process->thread->tf->psr = PSR_M_USR | PSR_F; // user mode, interrupts enabled
  process->thread->tf->pc  = entry;             // process entry point
  return arg1;
}

void
arch_trap_frame_pop(struct TrapFrame *tf)
{
  asm volatile(
    "mov     sp, %0\n"
    "b       trap_user_exit\n"
    :
    : "r" (tf)
    : "memory"
  );
}

int
arch_trap_is_user(struct TrapFrame *tf)
{
  return (tf->psr & PSR_M_MASK) == PSR_M_USR;
}
