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

#include <arch/i386/mmu.h>
#include <arch/i386/regs.h>


// Handle data or prefetch abort
static void
trap_handle_pgfault(struct TrapFrame *tf)
{
  uint32_t address;
  struct Process *process;

  address = cr2_get();

  // If abort happened in kernel mode, print the trap frame and panic
  if ((tf->cs & PL_MASK) != PL_USER) {
    print_trapframe(tf);
    k_panic("kernel fault va %p", address);
  }

  process = process_current();
  k_assert(process != NULL);

  // Try to handle VM fault first (it may be caused by copy-on-write pages)
  if (vm_handle_fault(process->vm->pgtab, address) == 0)
    return;

  // If unsuccessfull, kill the process
  //print_trapframe(tf);
  cprintf("[%d %s]: user fault va %p\n", process->pid, process->name, address);

  // TODO: SEGV_MAPERR or SEGV_ACCERR
  if (signal_generate(process->pid, SIGSEGV, 0) != 0)
    k_panic("sending SIGSEGV failed");
}

void
trap(struct TrapFrame *tf)
{
  struct Process *my_process = process_current();

  if ((tf->trapno >= T_IRQ0) && (tf->trapno < (T_IRQ0 + 32))) {
    interrupt_dispatch(tf);
  } else {
    switch (tf->trapno) {
    case T_PF:
      trap_handle_pgfault(tf);
      break;
    case T_SYSCALL:
      tf->eax = sys_dispatch();
      break;
    default:
      print_trapframe(tf);
      k_panic("unhandled trap in kernel");
    }
  }

  if ((tf->cs & PL_MASK) == PL_USER) {
    signal_deliver_pending();

    while (my_process->state != PROCESS_STATE_ACTIVE) {
      k_task_suspend();
      signal_deliver_pending();
    }
  }
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
  
  if ((tf->cs & PL_MASK) == PL_USER)
    cprintf("  ss     0x%08x    esp    0x%08x\n", tf->ss, tf->esp);
}

int
ipi_irq(int, void *)
{
  // TODO
  return -1;
}

int 
arch_trap_frame_init(struct Process *process, uintptr_t entry, uintptr_t arg1,
                     uintptr_t arg2, uintptr_t arg3, uintptr_t sp)
{
  k_assert(process->thread != NULL);

  sp -= 4;
  vm_copy_out(process->vm->pgtab, &arg3, sp, sizeof arg3);
  sp -= 4;
  vm_copy_out(process->vm->pgtab, &arg2, sp, sizeof arg2);
  sp -= 4;
  vm_copy_out(process->vm->pgtab, &arg1, sp, sizeof arg1);
  sp -= 4;

  process->thread->tf->cs = SEG_USER_CODE;
  process->thread->tf->eip = entry;
  process->thread->tf->es = SEG_USER_DATA;
  process->thread->tf->ds = SEG_USER_DATA;
  process->thread->tf->ss = SEG_USER_DATA;
  process->thread->tf->esp = sp;
  process->thread->tf->gs = 0;
  process->thread->tf->fs = 0;
  process->thread->tf->eflags = EFLAGS_IF;

  return 0;
}

void
arch_trap_frame_pop(struct TrapFrame *tf)
{
  asm volatile("movl %0,%%esp\n"
		"\tpopal\n"
    "\tpopl %%gs\n"
		"\tpopl %%fs\n"
		"\tpopl %%es\n"
		"\tpopl %%ds\n"
		"\taddl $0x8,%%esp\n"
		"\tiret"
		: : "g" (tf) : "memory");
	
  k_panic("failed");  /* mostly to placate the compiler */
}

int
arch_trap_is_user(struct TrapFrame *tf)
{
  return (tf->cs & PL_MASK) == PL_USER;
}
