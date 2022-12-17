#include <assert.h>

#include <argentum/armv7/regs.h>
#include <argentum/cpu.h>
#include <argentum/irq.h>

/*
 * ----------------------------------------------------------------------------
 * Per-CPU state
 * ----------------------------------------------------------------------------
 * 
 * The caller must disable interrupts while accessing the per-CPU data, since a
 * timer IRQ may cause the current thread to be moved to another processor, and
 * the pointer to the CPU struct will no longer be valid.
 *
 */

struct Cpu cpus[NCPU];

/**
 * Get the current processor ID.
 * 
 * @return The current processor ID.
 */
unsigned
cpu_id(void)
{
  return cp15_mpidr_get() & 3;
}

/**
 * Get the current CPU structure.
 *
 * @return The pointer to the current CPU structure.
 */
struct Cpu *
my_cpu(void)
{
  uint32_t psr;

  psr = cpsr_get();
  if (!(psr & PSR_I) || !(psr & PSR_F))
    panic("interruptible");

  return &cpus[cpu_id()];
}

struct KThread *
my_thread(void)
{
  struct KThread *thread;

  irq_save();
  thread = my_cpu()->thread;
  irq_restore();

  return thread;
}
