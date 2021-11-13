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
      process_destroy();
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
    panic("kernel fault address %08p status %08p", address, status);
  }

  // Abort happened in user mode.
  
  cprintf("user fault address %08p status %08p\n", address, status);
  process_destroy();
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
  static const char * const excnames[] = {
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
  cprintf("TRAP frame at %p from CPU %d\n", tf, read_mpidr() & 0x3);
  cprintf("  trap   = 0x%08x    (%s)\n", tf->trapno, trapname(tf->trapno));
  cprintf("  sp_usr = 0x%08x    lr_usr = 0x%08x\n", tf->sp_usr, tf->lr_usr);
  cprintf("  r0     = 0x%08x    r1     = 0x%08x\n", tf->r0,     tf->r1);
  cprintf("  r2     = 0x%08x    r3     = 0x%08x\n", tf->r2,     tf->r3);
  cprintf("  r4     = 0x%08x    r5     = 0x%08x\n", tf->r4,     tf->r5);
  cprintf("  r6     = 0x%08x    r7     = 0x%08x\n", tf->r6,     tf->r7);
  cprintf("  r8     = 0x%08x    r9     = 0x%08x\n", tf->r8,     tf->r9);
  cprintf("  r10    = 0x%08x    r11    = 0x%08x\n", tf->r10,    tf->r11);
  cprintf("  r12    = 0x%08x    psr    = 0x%08x\n", tf->r12,    tf->psr);
  cprintf("  lr     = 0x%08x    pc     = 0x%08x\n", tf->lr,     tf->pc);
}
