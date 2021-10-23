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

void
trap(struct Trapframe *tf)
{
  extern char *panicstr;
  unsigned irq;
  int resched;

  // Halt the CPU if some other CPU has calld panic().
  if (panicstr) {
    for (;;)
      ;
  }

  if (tf->trapno == T_SWI) {
    tf->r0 = syscall();
    return;
  }

  resched = 0;

  if (tf->trapno == T_IRQ) {
    irq = gic_intid();

    switch (irq & 0xFFFFFF) {
    case IRQ_PTIMER:
      ptimer_eoi();
      // cprintf("Timer IRQ from CPU %d\n", read_mpidr() & 0x3);
      resched = 1;
      break;
    case IRQ_UART0:
      uart_intr();
      break;
    case IRQ_KMI0:
      kmi_kbd_intr();
      break;
    }

    gic_eoi(irq);

    if (resched)
      process_yield();

    return;
  }

  print_trapframe(tf);

  panic("unhandled trap in kernel");
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

  // For MMU faults, print the contents of the corresponsing Fault Address
  // Register (FAR) and  the Fault Status Register (FSR).
  if (tf->trapno == T_DABT) {
    cprintf("  dfsr   = 0x%08x    dfar   = 0x%08x\n", read_dfsr(), read_dfar());
  } else if (tf->trapno == T_PABT) {
    cprintf("  ifsr   = 0x%08x    ifar   = 0x%08x\n", read_ifsr(), read_ifar());
  }

  cprintf("  sp_usr = 0x%08x    lr_usr = 0x%08x\n", tf->sp_usr, tf->lr_usr);
  cprintf("  r0     = 0x%08x    r1     = 0x%08x\n", tf->r0,     tf->r1);
  cprintf("  r2     = 0x%08x    r3     = 0x%08x\n", tf->r2,     tf->r3);
  cprintf("  r4     = 0x%08x    r5     = 0x%08x\n", tf->r4,     tf->r5);
  cprintf("  r6v    = 0x%08x    r7     = 0x%08x\n", tf->r6,     tf->r7);
  cprintf("  r8     = 0x%08x    r9     = 0x%08x\n", tf->r8,     tf->r9);
  cprintf("  r10    = 0x%08x    r11    = 0x%08x\n", tf->r10,    tf->r11);
  cprintf("  r12    = 0x%08x    psr    = 0x%08x\n", tf->r12,    tf->psr);
  cprintf("  lr     = 0x%08x    pc     = 0x%08x\n", tf->lr,     tf->pc);
}
