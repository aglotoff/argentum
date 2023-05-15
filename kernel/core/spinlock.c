#include <assert.h>
#include <stddef.h>
#include <string.h>

#include <kernel/armv7/regs.h>
#include <kernel/cprintf.h>
#include <kernel/cpu.h>
#include <kernel/irq.h>
#include <kernel/kdebug.h>
#include <kernel/process.h>
#include <kernel/spinlock.h>

static void arch_spin_lock(volatile int *);
static void arch_spin_unlock(volatile int *);
static void spin_pcs_save(struct SpinLock *);
static void spin_pcs_print(struct SpinLock *);

/**
 * Initialize a spinlock.
 * 
 * @param lock A pointer to the spinlock to be initialized.
 * @param name The name of the spinlock (for debugging purposes).
 */
void
spin_init(struct SpinLock *spin, const char *name)
{
  spin->locked = 0;
  spin->cpu    = NULL;
  spin->name   = name;
}

/**
 * Acquire the spinlock.
 *
 * @param lock A pointer to the spinlock to be acquired.
 */
void
spin_lock(struct SpinLock *spin)
{
  if (spin_holding(spin)) {
    spin_pcs_print(spin);
    panic("CPU %x is already holding %s", cpu_id(), spin->name);
  }

  // Disable interrupts to avoid deadlocks
  cpu_irq_save();

  arch_spin_lock(&spin->locked);

  spin->cpu = cpu_current();
  spin_pcs_save(spin);
}

/**
 * Release the spinlock.
 * 
 * @param lock A pointer to the spinlock to be released.
 */
void
spin_unlock(struct SpinLock *spin)
{
  if (!spin_holding(spin)) {
    spin_pcs_print(spin);
    panic("CPU %d cannot release %s: held by %d\n",
          cpu_id(), spin->name, spin->cpu);
  }

  spin->cpu = NULL;
  spin->pcs[0] = 0;

  arch_spin_unlock(&spin->locked);
  
  cpu_irq_restore();
}

/**
 * Check whether the current CPU is holding the lock.
 *
 * @param lock A pointer to the spinlock.
 * @return 1 if the current CPU is holding the lock, 0 otherwise.
 */
int
spin_holding(struct SpinLock *spin)
{
  int r;

  cpu_irq_save();
  r = spin->locked && (spin->cpu == cpu_current());
  cpu_irq_restore();

  return r;
}

// ARMv7-specific code to acquire a spinlock
static void
arch_spin_lock(volatile int *locked)
{
  int tmp1, tmp2;

  asm volatile(
    "\tmov     %2, #1\n"        // Load the "lock acquired" value
    "\t1:\n"
    "\tldrex   %1, [%0]\n"      // Read the lock field
    "\tcmp     %1, #0\n"        // Is the lock free?
    "\tbne     1b\n"            // No - try again
    "\tstrex   %1, %2, [%0]\n"  // Try and acquire the lock
    "\tcmp     %1, #0\n"        // Did this succeed?
    "\tbne     1b\n"            // No - try again
    : "+r"(locked), "=r"(tmp1), "=r"(tmp2)
    :
    : "memory", "cc");
}

// ARMv7-specific code to release a spinlock
static void
arch_spin_unlock(volatile int *locked)
{
  int tmp;

  asm volatile(
    "\tmov     %1, #0\n"
    "\tstr     %1, [%0]\n"
    : "+r"(locked), "=r"(tmp)
    :
    : "cc", "memory"
  );
}

// Record the current call stack by following the frame pointer chain.
// To properly generate stack backtrace structures, the code must be compiled
// with the -mapcs-frame and -fno-omit-frame-pointer flags
static void
spin_pcs_save(struct SpinLock *spin)
{
  uint32_t *fp;
  int i;

  fp = (uint32_t *) r11_get();

  for (i = 0; (fp != NULL) && (i < SPIN_MAX_PCS); i++) {
    spin->pcs[i] = fp[APCS_FRAME_LINK];
    fp = (uint32_t *) fp[APCS_FRAME_FP];
  }

  for ( ; i < SPIN_MAX_PCS; i++)
    spin->pcs[i] = 0;
}

// Display the recorded call stack along with debugging information
static void
spin_pcs_print(struct SpinLock *spin)
{
  int i;

  for (i = 0; i < SPIN_MAX_PCS && spin->pcs[i]; i++) {
    struct PcDebugInfo info;

    debug_info_pc(spin->pcs[i], &info);
    cprintf("  [%p] %s (%s at line %d)\n",
            spin->pcs[i],
            info.fn_name, info.file, info.line);
  }
}
