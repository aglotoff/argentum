#include <string.h>

#include <kernel/smp.h>
#include <kernel/irq.h>
#include <kernel/kernel.h>

#include <arch/kernel/mptimer.h>
#include <arch/kernel/realview_pbx_a9.h>
#include <arch/kernel/regs.h>
#include <arch/kernel/trap.h>

void
arch_trap_init(void)
{
  extern uint8_t vectors[], vectors_end[];

  memmove((void *) 0, vectors, vectors_end - vectors);
}

/**
 * Common entry point for all traps, including system calls. The TrapFrame
 * structure is built on the stack in trapentry.S
 */
void
arch_trap(struct TrapFrame *tf)
{
  // Halt if some other CPU already has called panic().
  if (panic_str) {
    for (;;) {
      asm volatile("wfi");
    }
  }

  // Dispatch based on what type of trap occured.
  switch (tf->trapno) {
  case T_IRQ:
    arch_irq_dispatch();
    break;
  default:
    // Either the user process misbehaved or the kernel has a bug.
    if ((tf->psr & PSR_M_MASK) == PSR_M_USR)
      panic("unhandled trap in user");

    arch_trap_print_frame(tf);
    panic("unhandled trap in kernel");
  }
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
arch_trap_print_frame(struct TrapFrame *tf)
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

  kprintf("TRAP frame at %p from CPU %d\n", tf, smp_id());
  kprintf("  psr  %p    [%s%s%s%s]\n",
          tf->psr,
          tf->psr & PSR_I ? "I," : "",
          tf->psr & PSR_I ? "F," : "",
          tf->psr & PSR_I ? "T," : "",
          modes[tf->psr & PSR_M_MASK] ? modes[tf->psr & PSR_M_MASK] : "");
  kprintf("  trap %p    [%s]\n", tf->trapno, get_trap_name(tf->trapno));
  kprintf("  sp   %p    lr   %p\n", tf->sp,  tf->lr);
  kprintf("  r0   %p    r1   %p\n", tf->r0,  tf->r1);
  kprintf("  r2   %p    r3   %p\n", tf->r2,  tf->r3);
  kprintf("  r4   %p    r5   %p\n", tf->r4,  tf->r5);
  kprintf("  r6   %p    r7   %p\n", tf->r6,  tf->r7);
  kprintf("  r8   %p    r9   %p\n", tf->r8,  tf->r9);
  kprintf("  r10  %p    r11  %p\n", tf->r10, tf->r11);
  kprintf("  r12  %p    pc   %p\n", tf->r12, tf->pc);
}
