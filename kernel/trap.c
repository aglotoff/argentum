#include <assert.h>
#include <stddef.h>

#include "armv7.h"
#include "console.h"
#include "gic.h"
#include "kmi.h"
#include "process.h"
#include "syscall.h"
#include "trap.h"
#include "vm.h"
#include "uart.h"

static void trap_handle_abort(struct Trapframe *);
static void trap_handle_irq(void);

void
trap(struct Trapframe *tf)
{
  extern char *panicstr;

  // Halt the CPU if some other CPU has called panic().
  if (panicstr) {
    for (;;)
      asm volatile("wfi");
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
    trap_handle_irq();
    break;
  default:
    // Unexpected trap: either the user process or the kernel has a bug.
    if ((tf->psr & PSR_M_MASK) == PSR_M_USR) {
      process_destroy(-1);
    }
    print_trapframe(tf);
    panic("unhandled trap in kernel");
  }
}

static void
trap_handle_abort(struct Trapframe *tf)
{
  uint32_t address, status;

  // Read the contents of the corresponsing Fault Address Register (FAR) and 
  // the Fault Status Register (FSR).
  address = tf->trapno == T_DABT ? read_dfar() : read_ifar();
  status  = tf->trapno == T_DABT ? read_dfsr() : read_ifsr();

  if ((tf->psr & PSR_M_MASK) != PSR_M_USR) {
    // Kernel-mode abort.
    print_trapframe(tf);
    panic("kernel fault va %p status %#x", address, status);
  }

  // Abort happened in user mode.
  
  cprintf("user fault va %p status %#x\n", address, status);
  process_destroy(-1);
}

static void
trap_handle_irq(void)
{
  int irq, resched;

  irq = gic_intid();
  resched = 0;

  switch (irq & 0xFFFFFF) {
  case IRQ_PTIMER:
    ptimer_eoi();
    resched = 1;
    break;
  case IRQ_UART0:
    uart_intr();
    break;
  case IRQ_KMI0:
    kmi_kbd_intr();
    break;
  default:
    cprintf("Unexpected IRQ %d from CPU %d\n", irq & 0xFFFFFF, cpuid());
    break;
  }

  gic_eoi(irq);

  if (resched)
    process_yield();
}

static const char *
trapname(unsigned trapno)
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
print_trapframe(struct Trapframe *tf)
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

  cprintf("TRAP frame at \x1b[1;36m%p\x1b[0m from \x1b[1;36mCPU %d\x1b[0m\n",
          tf, cpuid());
  
  cprintf("  \x1b[1;33mtrap\x1b[0m %p    \x1b[1;35m<%s>\x1b[0m\n",
          tf->trapno, trapname(tf->trapno));

  cprintf("  \x1b[1;33mpsr\x1b[0m  %p    \x1b[1;35m<%s%s%s%s>\x1b[0m\n",
          tf->psr,
          tf->psr & PSR_I ? "I," : "",
          tf->psr & PSR_I ? "F," : "",
          tf->psr & PSR_I ? "T," : "",
          modes[tf->psr & PSR_M_MASK] ? modes[tf->psr & PSR_M_MASK] : "");

  cprintf("  \x1b[1;33mr0\x1b[0m   %p  "
          "  \x1b[1;33mr1\x1b[0m   %p\n", 
          tf->r0,     tf->r1);
  cprintf("  \x1b[1;33mr2\x1b[0m   %p  "
          "  \x1b[1;33mr3\x1b[0m   %p\n", 
          tf->r2,     tf->r3);
  cprintf("  \x1b[1;33mr4\x1b[0m   %p  "
          "  \x1b[1;33mr5\x1b[0m   %p\n", 
          tf->r4,     tf->r5);
  cprintf("  \x1b[1;33mr6\x1b[0m   %p  "
          "  \x1b[1;33mr7\x1b[0m   %p\n", 
          tf->r6,     tf->r7);
  cprintf("  \x1b[1;33mr8\x1b[0m   %p  "
          "  \x1b[1;33mr9\x1b[0m   %p\n", 
          tf->r8,     tf->r9);
  cprintf("  \x1b[1;33mr10\x1b[0m  %p  "
          "  \x1b[1;33mr11\x1b[0m  %p\n", 
          tf->r10,    tf->r11);
  cprintf("  \x1b[1;33mr12\x1b[0m  %p  ",
          tf->r12);
  cprintf("  \x1b[1;33msp\x1b[0m   %p\n"
          "  \x1b[1;33mlr\x1b[0m   %p  ", 
          (tf->psr & PSR_M_MASK) != PSR_M_SVC ? (void *) tf->sp_usr : (tf + 1),
          (tf->psr & PSR_M_MASK) != PSR_M_SVC ? tf->lr_usr : tf->lr);
  cprintf("  \x1b[1;33mpc\x1b[0m   \x1b[1;35m%p\x1b[0m\n", 
          tf->pc);
}
