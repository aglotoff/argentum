#include <kernel/core/assert.h>
#include <kernel/core/cpu.h>
#include <kernel/core/irq.h>

#include "core_private.h"

// TODO: should be architecture-specific
#define K_CPU_MAX   4

static struct KCpu _k_cpus[K_CPU_MAX];

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
  k_assert(!k_arch_irq_is_enabled());
  return &_k_cpus[k_cpu_id()];
}
