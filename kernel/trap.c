#include <signal.h>

#include <kernel/armv7/regs.h>
#include <kernel/cprintf.h>
#include <kernel/page.h>
#include <kernel/vmspace.h>
#include <kernel/process.h>
#include <kernel/sys.h>
#include <kernel/trap.h>
#include <kernel/types.h>
#include <kernel/irq.h>
#include <kernel/monitor.h>
#include <kernel/semaphore.h>
#include <kernel/mach.h>

static void trap_handle_abort(struct TrapFrame *);

/**
 * Common entry point for all traps, including system calls. The TrapFrame
 * structure is built on the stack in trapentry.S
 */
void
trap(struct TrapFrame *tf)
{
  extern char *panicstr;

  struct KThread *my_thread = k_thread_current();
  struct Process *my_process = my_thread ? my_thread->process : NULL;

  // Halt if some other CPU already has called panic().
  if (panicstr) {
    for (;;) {
      asm volatile("wfi");
    }
  }

  if (my_thread && (uintptr_t) (my_thread->kstack) >= (uintptr_t) tf)
    panic("stack underflow %p %p", my_thread->kstack, tf);

  // User-mode trap frame address should never change, there's logic in the
  // kernel that relies on this!
  if (((tf->psr & PSR_M_MASK) == PSR_M_USR) && (my_process->thread->tf != tf))
    panic("user-mode trap frame address unexpectedly changed");

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
    interrupt_dispatch();
    break;
  case T_UNDEF:
    if ((tf->psr & PSR_M_MASK) == PSR_M_USR) {
      // TODO: ILL_ILLOPC
      if (signal_generate(my_process->pid, SIGILL, 0) != 0)
        panic("sending SIGILL failed");
      break;
    }
    // fall through
  default:
    print_trapframe(tf);
    panic("unhandled trap in kernel");
  }

  if ((tf->psr & PSR_M_MASK) == PSR_M_USR) {
    signal_deliver_pending();

    while (my_process->state != PROCESS_STATE_ACTIVE) {
      k_thread_suspend();
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
    panic("kernel fault va %p status %#x", address, status);
  }

  process = process_current();
  assert(process != NULL);

  // Try to handle VM fault first (it may be caused by copy-on-write pages)
  if (((status & 0xF) == 0xF) && vm_handle_fault(process->vm->pgtab, address) == 0) {
    return;
  }

  // If unsuccessfull, kill the process
  print_trapframe(tf);
  panic("[%d %s]: user fault va %p status %#x\n", process->pid, process->name, address, status);

  // TODO: SEGV_MAPERR or SEGV_ACCERR
  if (signal_generate(process->pid, SIGSEGV, 0) != 0)
    panic("sending SIGSEGV failed");
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
timer_irq(void *)
{
  struct Process *my_process = process_current();

  // cprintf("Tick!\n");

  if (my_process != NULL) {
    if ((my_process->thread->tf->psr & PSR_M_MASK) != PSR_M_USR) {
      process_update_times(my_process, 0, 1);
    } else {
      process_update_times(my_process, 1, 0);
    }
  }

  tick();

  return 1;
}

int
ipi_irq(void *)
{
  return 1;
}

void
interrupt_ipi(void)
{
  mach_current->interrupt_ipi();
}

static int
interrupt_id(void)
{
  return mach_current->interrupt_id();
}

void
interrupt_enable(int irq, int cpu)
{
  mach_current->interrupt_enable(irq, cpu);
}

void
interrupt_mask(int irq)
{
  mach_current->interrupt_mask(irq);
}

void
interrupt_unmask(int irq)
{
  mach_current->interrupt_unmask(irq);
}

void
interrupt_init(void)
{
  mach_current->interrupt_init();
  mach_current->timer_init();
}

void
interrupt_init_percpu(void)
{
  mach_current->interrupt_init_percpu();
  mach_current->timer_init_percpu();
}

static void
irq_eoi(int irq)
{
  mach_current->interrupt_eoi(irq);
}

void
interrupt_dispatch(void)
{
  int irq = interrupt_id();

  k_irq_begin();

  interrupt_mask(irq);
  irq_eoi(irq);

  // k_irq_enable();

  // Enable nested interrupts
  if (k_irq_dispatch(irq & 0xFFFFFF))
    interrupt_unmask(irq);

  k_irq_end();
}

static int
interrupt_common(void *arg)
{
  struct ISRThread *isr = (struct ISRThread *) arg;

  k_semaphore_put(&isr->semaphore);
  return 0;
}

static void
interrupt_thread(void *arg)
{
  struct ISRThread *isr = (struct ISRThread *) arg;

  for (;;) {
    if (k_semaphore_get(&isr->semaphore) < 0)
      panic("k_semaphore_get");

    isr->handler(isr->handler_arg);
  }
}

void
interrupt_attach_thread(struct ISRThread *isr, int irq, void (*handler)(void *), void *handler_arg)
{
  struct KThread *thread;

  if ((thread = k_thread_create(NULL, interrupt_thread, isr, 0)) == NULL)
    panic("Cannot create IRQ thread");

  k_semaphore_init(&isr->semaphore, 0);
  isr->handler     = handler;
  isr->handler_arg = handler_arg;

  k_irq_attach(irq, interrupt_common, isr);

  k_thread_resume(thread);
}
