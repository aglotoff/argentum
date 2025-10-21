#include <kernel/core/assert.h>
#include <kernel/core/cpu.h>
#include <kernel/core/irq.h>

#include "core_private.h"

static struct KCpu _k_cpus[K_CPU_MAX];

struct KCpu *
_k_cpu(void)
{
  k_assert(!k_arch_irq_is_enabled());
  return &_k_cpus[k_cpu_id()];
}
