#include <kernel/core/cpu.h>
#include <kernel/armv7/regs.h>

// In case of four Cortex-A9 processors, the CPU IDs are 0x0, 0x1, 0x2, and
// 0x3 so we can use them as array indicies.
unsigned
k_arch_cpu_id(void)
{
  return cp15_mpidr_get() & CP15_MPIDR_CPU_ID;
}
