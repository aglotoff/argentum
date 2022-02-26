#include <assert.h>
#include <stddef.h>
#include <string.h>

#include <kernel/armv7.h>
#include <kernel/cprintf.h>
#include <kernel/cpu.h>
#include <kernel/drivers/console.h>
#include <kernel/drivers/eth.h>
#include <kernel/drivers/gic.h>
#include <kernel/drivers/kbd.h>
#include <kernel/drivers/sd.h>
#include <kernel/drivers/uart.h>
#include <kernel/mm/page.h>
#include <kernel/mm/vm.h>
#include <kernel/process.h>
#include <kernel/syscall.h>
#include <kernel/types.h>

#include <kernel/trap.h>

static void trap_handle_abort(struct Trapframe *);
static void trap_irq_dispatch(void);

void
trap(struct Trapframe *tf)
{
  extern char *panicstr;

  // Halt if some other CPU already has called panic().
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
    trap_irq_dispatch();
    break;
  default:
    // Either the user process misbehaved or the kernel has a bug.
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
  struct PageInfo *fault_page, *page;
  tte_t *trtab;
  pte_t *pte;

  // Read the contents of the corresponsing Fault Address Register (FAR) and 
  // the Fault Status Register (FSR).
  address = tf->trapno == T_DABT ? read_dfar() : read_ifar();
  status  = tf->trapno == T_DABT ? read_dfsr() : read_ifsr();

  if ((tf->psr & PSR_M_MASK) != PSR_M_USR) {
    // Kernel-mode abort.
    print_trapframe(tf);
    panic("kernel fault va %p status %#x", address, status);
  }

  trtab = my_process()->vm.trtab;

  if ((fault_page = vm_lookup_page(trtab, (void *) address, &pte)) != NULL) {
    int prot;

    prot = vm_pte_get_flags(pte);
    if ((prot & VM_COW) && ((page = page_alloc(0)) != NULL)) {
      memcpy(page2kva(page), page2kva(fault_page), PAGE_SIZE);

      prot &= ~VM_COW;
      prot |= VM_WRITE;

      if (vm_insert_page(trtab, page, (void *) address, prot) == 0)
        return;
    }
  }

  // Abort happened in user mode.
  cprintf("user fault va %p status %#x\n", address, status);
  process_destroy(-1);
}

static void
trap_irq_dispatch(void)
{
  int irq, resched;

  irq_save();

  // Read the interrupt ID and temporarily disable it.
  irq = gic_intid();
  gic_disable(irq);
  gic_eoi(irq);

  irq_restore();

  resched = 0;

  // Process the IRQ.
  switch (irq & 0xFFFFFF) {
  case 0:
    // TODO:
    break;
  case IRQ_PTIMER:
    ptimer_intr();
    resched = 1;
    break;
  case IRQ_UART0:
    uart_intr();
    break;
  case IRQ_KMI0:
    kbd_intr();
    break;
  case IRQ_MCIA:
    sd_intr();
    break;
  case IRQ_ETH:
    eth_intr();
    break;
  default:
    cprintf("Unexpected IRQ %d from CPU %d\n", irq & 0xFFFFFF, cpu_id());
    break;
  }

  // Re-enable the IRQ
  gic_enable(irq, cpu_id());

  if (resched && (my_process() != NULL))
    process_yield();
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

  cprintf("TRAP frame at %p from CPU %d\n",
          tf, cpu_id());
  
  cprintf("  trap %p    [%s]\n",
          tf->trapno, trap_name(tf->trapno));

  cprintf("  psr  %p    [%s%s%s%s]\n",
          tf->psr,
          tf->psr & PSR_I ? "I," : "",
          tf->psr & PSR_I ? "F," : "",
          tf->psr & PSR_I ? "T," : "",
          modes[tf->psr & PSR_M_MASK] ? modes[tf->psr & PSR_M_MASK] : "");

  cprintf("  r0   %p  "
          "  r1   %p\n", 
          tf->r0,     tf->r1);
  cprintf("  r2   %p  "
          "  r3   %p\n", 
          tf->r2,     tf->r3);
  cprintf("  r4   %p  "
          "  r5   %p\n", 
          tf->r4,     tf->r5);
  cprintf("  r6   %p  "
          "  r7   %p\n", 
          tf->r6,     tf->r7);
  cprintf("  r8   %p  "
          "  r9   %p\n", 
          tf->r8,     tf->r9);
  cprintf("  r10  %p  "
          "  r11  %p\n", 
          tf->r10,    tf->r11);
  cprintf("  r12  %p  ",
          tf->r12);
  cprintf("  sp   %p\n"
          "  lr   %p  ", 
          (tf->psr & PSR_M_MASK) != PSR_M_SVC ? (void *) tf->sp_usr : (tf + 1),
          (tf->psr & PSR_M_MASK) != PSR_M_SVC ? tf->lr_usr : tf->lr);
  cprintf("  pc   %p\n", 
          tf->pc);
}
