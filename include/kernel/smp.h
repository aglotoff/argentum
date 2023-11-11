#ifndef __AG_INCLUDE_KERNEL_SMP_H__
#define __AG_INCLUDE_KERNEL_SMP_H__

#ifndef __AG_KERNEL__
#error "This is an Argentum kernel header; user programs should not include it"
#endif

#include <arch/kernel/smp.h>
#include <kernel/kernel.h>

/** At most four CPUs on Cortex-A9 MPCore */
#define SMP_CPU_MAX   4

#ifndef __ASSEMBLER__

/**
 * The Argentum kernel maintains a special structure for each processor, which
 * records the per-CPU information.
 */
struct Cpu {
  /** Nesting level of cpu_irq_save() calls */
  int             irq_save_count;
  /** IRQ state before the first cpu_irq_save() */
  int             irq_flags;
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get the current processor ID.
 * 
 * @return The current processor ID.
 */
static inline unsigned
smp_id(void)
{
  return arch_smp_id();
}

struct Cpu *smp_cpu(void);

#ifdef __cplusplus
};
#endif

#endif // !__ASSEMBLER__

#endif  // !__AG_INCLUDE_KERNEL_SMP_H__
