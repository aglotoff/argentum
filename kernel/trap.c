#include <argentum/armv7/regs.h>
#include <argentum/cprintf.h>
#include <argentum/mm/page.h>
#include <argentum/mm/vm.h>
#include <argentum/process.h>
#include <argentum/sys.h>
#include <argentum/trap.h>
#include <argentum/types.h>
#include <argentum/irq.h>

static void trap_handle_abort(struct TrapFrame *);

void
trap(struct TrapFrame *tf)
{
  extern char *panicstr;

  // Halt if some other CPU already has called panic().
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
    tf->r0 = sys_dispatch();
    break;
  case T_IRQ:
    irq_dispatch();
    break;
  default:
    // Either the user process misbehaved or the kernel has a bug.
    if ((tf->psr & PSR_M_MASK) == PSR_M_USR)
      process_destroy(-1);

    print_trapframe(tf);
    panic("unhandled trap in kernel");
  }
}

static void
trap_handle_abort(struct TrapFrame *tf)
{
  uint32_t address, status;
  struct Process *process;

  // Read the contents of the corresponsing Fault Address Register (FAR) and 
  // the Fault Status Register (FSR).
  address = tf->trapno == T_DABT ? cp15_dfar_get() : cp15_ifar_get();
  status  = tf->trapno == T_DABT ? cp15_ifsr_get() : cp15_ifsr_get();

  // If abort happened in kernel mode, print the trap frame and call panic()
  if ((tf->psr & PSR_M_MASK) != PSR_M_USR) {
    print_trapframe(tf);
    panic("kernel fault va %p status %#x", address, status);
  }

  process = my_process();
  assert(process != NULL);

  if (vm_handle_fault(process->vm, address) == 0)
    return;

  // Abort happened in user mode.
  cprintf("user fault va %p status %#x\n", address, status);
  process_destroy(-1);
}

static const char *
trap_name(unsigned trapno)
{
  static const char *const excnames[] = {
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
    return excnames[trapno];
  return "(unknown trap)";
}

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

  cprintf("TRAP frame at %p from CPU %d\n", tf, cpu_id());
  cprintf("  psr  %p    [%s%s%s%s]\n",
          tf->psr,
          tf->psr & PSR_I ? "I," : "",
          tf->psr & PSR_I ? "F," : "",
          tf->psr & PSR_I ? "T," : "",
          modes[tf->psr & PSR_M_MASK] ? modes[tf->psr & PSR_M_MASK] : "");
  cprintf("  trap %p    [%s]\n", tf->trapno, trap_name(tf->trapno));
  cprintf("  sp   %p    lr   %p\n", tf->sp,  tf->lr);
  cprintf("  r0   %p    r1   %p\n", tf->r0,  tf->r1);
  cprintf("  r2   %p    r3   %p\n", tf->r2,  tf->r3);
  cprintf("  r4   %p    r5   %p\n", tf->r4,  tf->r5);
  cprintf("  r6   %p    r7   %p\n", tf->r6,  tf->r7);
  cprintf("  r8   %p    r9   %p\n", tf->r8,  tf->r9);
  cprintf("  r10  %p    r11  %p\n", tf->r10, tf->r11);
  cprintf("  r12  %p    pc   %p\n", tf->r12, tf->pc);
}
