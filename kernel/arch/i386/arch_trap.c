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
#include <kernel/interrupt.h>
#include <kernel/time.h>
#include <kernel/signal.h>

void
trap(struct TrapFrame *tf)
{
  if ((tf->trapno >= T_IRQ0) && (tf->trapno < (T_IRQ0 + 16))) {
    interrupt_dispatch(tf);
    return;
  }

  print_trapframe(tf);
  panic("unhandled trap");
}

// Returns a human-readable name for the given trap number
static const char *
get_trap_name(unsigned trapno)
{
  static const char *const known_traps[] = {
    [T_DE]  = "Divide error",
    [T_DB]  = "Reserved",
    [T_NMI] = "NMI Interrupt",
    [T_BP]  = "Breakpoint",
    [T_OF]  = "Overflow",
    [T_BR]  = "BOUND Range Exceeded",
    [T_UD]  = "Undefined Opcode",
    [T_NM]  = "No Math Coprocessor",
    [T_DF]  = "Double Fault",
    [T_CSS] = "Coprocessor Segment Overrun",
    [T_TS]  = "Invalid TSS",
    [T_NP]  = "Segment Not Present",
    [T_SS]  = "Stack-Segment Fault",
    [T_GP]  = "General Protection Fault",
    [T_PF]  = "Page Fault",
    [T_MF]  = "Math Fault",
    [T_AC]  = "Alignment Check",
    [T_MC]  = "Machine Check",
    [T_XF]  = "SIMD Floating-Point Exception",
  };

  if ((trapno <= T_XF) && (known_traps[trapno] != NULL))
    return known_traps[trapno];

  return "(unknown trap)";
}

void
print_trapframe(struct TrapFrame *tf)
{
  cprintf("TRAP frame at %p from CPU %d\n", tf, k_cpu_id());

  cprintf("  eflags 0x%08x    cs     0x%08x\n", tf->eflags, tf->cs);
  cprintf("  eip    0x%08x    error  0x%08x\n", tf->eip, tf->error);
  cprintf("  trap   0x%08x    [%s]\n", tf->trapno, get_trap_name(tf->trapno));
  cprintf("  ds     0x%08x    es     0x%08x\n", tf->ds, tf->es);
  cprintf("  fs     0x%08x    gs     0x%08x\n", tf->fs, tf->gs);
  cprintf("  eax    0x%08x    ecx    0x%08x\n", tf->eax, tf->ecx);
  cprintf("  edx    0x%08x    ebx    0x%08x\n", tf->edx, tf->ebx);
  cprintf("  _esp   0x%08x    ebp    0x%08x\n", tf->_esp, tf->ebp);
  cprintf("  esi    0x%08x    edi    0x%08x\n", tf->esi, tf->edi);
  
  // TODO: ss, esp
}

int
ipi_irq(int, void *)
{
  // TODO
  return -1;
}

int 
arch_trap_frame_init(struct TrapFrame *tf, uintptr_t entry, uintptr_t arg1,
                     uintptr_t arg2, uintptr_t arg3, uintptr_t sp)
{
  // TODO
  (void) tf;
  (void) entry;
  (void) arg1;
  (void) arg2;
  (void) arg3;
  (void) sp;
  return -1;
}

void
arch_trap_frame_pop(struct TrapFrame *tf)
{
  // TODO:
  (void) tf;
}
