#include <assert.h>
#include <stddef.h>

#include <kernel/kernel.h>
#include <kernel/irq.h>
#include <kernel/smp.h>
#include <kernel/spinlock.h>
#include <kernel/vm.h>

#include <arch/kernel/realview_pbx_a9.h>
#include <arch/kernel/regs.h>

static struct Cpu cpus[SMP_CPU_MAX];

void
arch_smp_init(void)
{
  extern uint8_t _start[];

  // The boot code for realview-pbx-a9 enables GIC for secondary CPUs and puts
  // them into a loop waiting for an IPI and checking the SYS_FLAGS register for
  // an address to jump to (application can assign any meaning to SYS_FLAGS)
  *(volatile uintptr_t *) PA2KVA(SYS_FLAGSSET) = (uintptr_t) _start;
  arch_irq_ipi_others(0);
}

void
arch_smp_main(void)
{
  arch_vm_init_percpu();
  arch_irq_init_percpu();

  // TODO: start the scheduler
  irq_enable();

  for (;;) {
    asm volatile("wfi");
  }
}

unsigned
arch_smp_id(void)
{
  return cp15_mpidr_get() & MPIDR_CPU_ID;
}

struct Cpu *
arch_smp_get_cpu(unsigned id)
{
  // In case of four Cortex-A9 processors, the CPU IDs are 0x0, 0x1, 0x2, and
  // 0x3 so we can use them as array indicies.
  return &cpus[id];
}
