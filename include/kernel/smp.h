#ifndef __AG_INCLUDE_KERNEL_SMP_H__
#define __AG_INCLUDE_KERNEL_SMP_H__

#ifndef __AG_KERNEL__
#error "This is an Argentum kernel header; user programs should not include it"
#endif

#include <kernel/kernel.h>

#include <arch/kernel/irq.h>
#include <arch/kernel/smp.h>

/** At most four CPUs on Cortex-A9 MPCore */
#define SMP_CPU_MAX   4

#ifndef __ASSEMBLER__

struct Thread;

/**
 * The Argentum kernel maintains a special structure for each processor, which
 * records the per-CPU information.
 */
struct Cpu {
  /** The thread currently running on this CPU */
  struct Thread  *thread;
  /** Saved architecture-specific scheduler context */
  void           *sched_context;
  /** Nesting level of cpu_irq_save() calls */
  int             irq_save_count;
  /** IRQ state before the first cpu_irq_save() */
  int             irq_flags;
  /** IRQ handler nesting level */
  int             irq_handler_level;
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

/**
 * Get the current CPU structure. The caller must disable interrupts, otherwise
 * the task could move to a different processor due to timer interrupt. 
 *
 * @return Pointer to the current CPU structure.
 */
static inline struct Cpu *
smp_cpu(void)
{
  if (arch_irq_is_enabled())
    panic("interruptible");
  return arch_smp_get_cpu(arch_smp_id());
}

#ifdef __cplusplus
};
#endif

#endif // !__ASSEMBLER__

#endif  // !__AG_INCLUDE_KERNEL_SMP_H__
