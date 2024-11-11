#ifndef __KERNEL_INCLUDE_KERNEL_CORE_CPU_H__
#define __KERNEL_INCLUDE_KERNEL_CORE_CPU_H__

unsigned k_arch_cpu_id(void);

static inline unsigned
k_cpu_id(void)
{
  return k_arch_cpu_id();
}

#endif  // !__KERNEL_INCLUDE_KERNEL_CORE_CPU_H__
