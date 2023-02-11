#ifndef __INCLUDE_ARGENTUM_CPU_H__
#define __INCLUDE_ARGENTUM_CPU_H__

#ifndef __AG_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

struct Context;
struct KThread;

/**
 * Per-CPU state.
 */
struct Cpu {
  struct Context *scheduler;      ///< Saved scheduler context
  struct KThread *thread;         ///< The currently running kernel thread
  int             isr_nesting;    ///< ISR nesting level
  int             irq_save_count; ///< Depth of cpu_irq_save() nesting
  int             irq_flags;      ///< Were interrupts enabled before IRQ save?
};

/**
 * Maximum number of CPUs.
 */
#define NCPU  4

extern struct Cpu cpus[];

struct Cpu     *my_cpu(void);
struct KThread *my_thread(void);
unsigned        cpu_id(void);
void            cpu_irq_disable(void);
void            cpu_irq_enable(void);
void            cpu_irq_save(void);
void            cpu_irq_restore(void);
void            cpu_isr_enter(void);
void            cpu_isr_exit(void);

#endif  // !__INCLUDE_ARGENTUM_CPU_H__
