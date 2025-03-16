#include <kernel/core/assert.h>
#include <kernel/core/cpu.h>
#include <arch/i386/lapic.h>

unsigned
k_arch_cpu_id(void)
{
  k_assert(lapic_id() == 0);
  //return lapic_id();
  return 0;
}
