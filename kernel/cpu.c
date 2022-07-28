#include <assert.h>

#include <armv7/regs.h>

#include <cpu.h>

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


/*
 * ----------------------------------------------------------------------------
 * Interrupt state control routines
 * ----------------------------------------------------------------------------
 * 
 * irq_save() and irq_restore() are used to disable and reenable interrupts on
 * the current CPU, respectively. Their invocations are counted, i.e. it takes
 * two irq_restore() calls to undo two irq_save() calls. This allows, for
 * example, to acquire two different locks and the interrupts will not be
 * reenabled until both locks have been released.
 *
 */

void
irq_disable(void)
{
  cpsr_set(cpsr_get() | PSR_I | PSR_F);
}

void
irq_enable(void)
{
  cpsr_set(cpsr_get() & ~(PSR_I | PSR_F));
}

/**
 * Save the current CPU interrupt state and disable interrupts.
 *
 * Both IRQ and FIQ interrupts are being disabled.
 */
void
irq_save(void)
{
  uint32_t psr;

  psr = cpsr_get();
  cpsr_set(psr | PSR_I | PSR_F);

  if (my_cpu()->irq_save_count++ == 0)
    my_cpu()->irq_flags = ~psr & (PSR_I | PSR_F);
}

/**
 * Restore the interrupt state saved by a preceding irq_save() call.
 */
void
irq_restore(void)
{
  uint32_t psr;

  psr = cpsr_get();
  if (!(psr & PSR_I) || !(psr & PSR_F))
    panic("interruptible");

  if (--my_cpu()->irq_save_count < 0)
    panic("interruptible");

  if (my_cpu()->irq_save_count == 0)
    cpsr_set(psr & ~my_cpu()->irq_flags);
}
