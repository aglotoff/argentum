#ifndef __KERNEL_INCLUDE_KERNEL_CPU_H__
#define __KERNEL_INCLUDE_KERNEL_CPU_H__

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

/**
 * At most four CPUs on Cortex-A9 MPCore.
 */
#define NCPU      4

unsigned k_arch_cpu_id(void);

/**
 * Get the current processor ID.
 * 
 * @return The current processor ID.
 */
static inline unsigned
k_cpu_id(void)
{
  return k_arch_cpu_id();
}

#endif  // !__KERNEL_INCLUDE_KERNEL_CPU_H__
