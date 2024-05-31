#ifndef __KERNEL_INCLUDE_KERNEL_CPU_H__
#define __KERNEL_INCLUDE_KERNEL_CPU_H__

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <kernel/armv7/regs.h>

struct Context;
struct KThread;

/**
 * The kernel maintains a special structure for each processor, which
 * records the per-CPU information.
 */
struct KCpu {
  struct Context *sched_context;  ///< Saved scheduler context
  struct KThread *thread;         ///< The currently running kernel task
  int             isr_nesting;    ///< ISR nesting level
  int             irq_save_count; ///< Nesting level of k_irq_save() calls
  int             irq_flags;      ///< IRQ state before the first k_irq_save()
};

/**
 * At most four CPUs on Cortex-A9 MPCore.
 */
#define NCPU      4

extern struct KCpu _cpus[];

struct KCpu    *k_cpu(void);

void            k_irq_disable(void);
void            k_irq_enable(void);
void            k_irq_save(void);
void            k_irq_restore(void);

/**
 * Get the current processor ID.
 * 
 * @return The current processor ID.
 */
static inline unsigned
k_cpu_id(void)
{
  return cp15_mpidr_get() & CP15_MPIDR_k_cpu_id;
}

#endif  // !__KERNEL_INCLUDE_KERNEL_CPU_H__
