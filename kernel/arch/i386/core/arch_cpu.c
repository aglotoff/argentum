#include <kernel/core/cpu.h>

unsigned
k_arch_cpu_id(void)
{
  // FIXME: add SMP support
  return 0;
}
