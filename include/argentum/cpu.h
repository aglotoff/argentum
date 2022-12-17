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
  struct KThread  *thread;         ///< The currently running thread   
  int             irq_save_count; ///< Depth of irq_save() nesting
  int             irq_flags;      ///< Were interupts enabled before IRQ save?
};

/**
 * Maximum number of CPUs.
 */
#define NCPU  4

extern struct Cpu cpus[];

unsigned     cpu_id(void);
struct Cpu  *my_cpu(void);
struct KThread *my_thread(void);

#endif  // !__INCLUDE_ARGENTUM_CPU_H__
