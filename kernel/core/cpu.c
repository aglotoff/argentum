#include <kernel/assert.h>
#include <kernel/cpu.h>
#include <kernel/irq.h>

#include <kernel/armv7/regs.h>

#include "core_private.h"

struct KCpu _cpus[NCPU];

/**
 * Get the current CPU structure.
 * 
 * The caller must disable interrupts, otherwise the task could switch to a
 * different processor due to timer interrupt and the return value will be
 * incorrect. 
 *
 * @return The pointer to the current CPU structure.
 */
struct KCpu *
_k_cpu(void)
{
  if (k_arch_irq_is_enabled())
    panic("interruptible");

  // In case of four Cortex-A9 processors, the CPU IDs are 0x0, 0x1, 0x2, and
  // 0x3 so we can use them as array indicies.
  return &_cpus[k_cpu_id()];
}

unsigned
k_arch_cpu_id(void)
{
  return cp15_mpidr_get() & CP15_MPIDR_CPU_ID;
}
