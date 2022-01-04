#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "armv7.h"
#include "console.h"
#include "cpu.h"
#include "kdebug.h"
#include "process.h"
#include "spinlock.h"

static void spin_get_caller_pcs(struct Spinlock *);
static void spin_print_caller_pcs(struct Spinlock *);

void
spin_init(struct Spinlock *lock, const char *name)
{
  lock->locked = 0;
  lock->cpu    = NULL;
  lock->name   = name;
}

/**
 * Acquire the lock.
 * 
 * @param lock The lock to be acquired.
 */
void
spin_lock(struct Spinlock *lock)
{
  int t1, t2;

  // Disable interrupts to avoid deadlock.
  irq_save();

  if (spin_holding(lock)) {
    cprintf("CPU %d is already holding %s\n", cpu_id(), lock->name);
    spin_print_caller_pcs(lock);
    panic("spin_lock");
  }

  asm volatile(
    "\t1:\n"
    "\tldrex   %1, [%0]\n"      // Read the lock field
    "\tcmp     %1, #0\n"        // Compare with 0
    "\twfene\n"                 // Not 0 means already locked, do WFE
    "\tbne     1b\n"            // Retry after woken up by event
    "\tmov     %1, #1\n" 
    "\tstrex   %2, %1, [%0]\n"  // Try to store 1 into the lock field
    "\tcmp     %2, #0\n"        // Check return value: 0=OK, 1=failed
    "\tbne     1b\n"            // If store failed, try again
    "\tdmb\n"                   // Memory barrier BEFORE accessing the resource
    : "+r"(lock), "=r"(t1), "=r"(t2)
    :
    : "memory", "cc");

  // Record info about lock acquisition for debugging.
  lock->cpu = my_cpu();
  spin_get_caller_pcs(lock);
}

/**
 * Release the lock.
 * 
 * @param lock The lock to be released.
 */
void
spin_unlock(struct Spinlock *lock)
{
  int t;

  if (!spin_holding(lock)) {
    cprintf("CPU %d cannot release %s: held by %d\n",
            cpu_id(), lock->name, lock->cpu);
    spin_print_caller_pcs(lock);
    panic("spin_unlock");
  }

  lock->cpu = NULL;
  lock->pcs[0] = 0;

  asm volatile(
    "\tmov     %1, #0\n"
    "\tdmb\n"                   // Memory barier BEFORE releasing the resource
    "\tstr     %1, [%0]\n"      // Write 0 into the lock field
    "\tdsb\n"                   // Ensure update has completed before SEV
    "\tsev\n"                   // Send event to wakeup other CPUS in WFE mode
    : "+r"(lock), "=r"(t)
    :
    : "cc", "memory"
  );
  
  irq_restore();
}

/**
 * Check whether the current CPU is holding the lock.
 *
 * @param lock The spinlock.
 * @return 1 if the current CPU is holding the lock, 0 otherwise.
 */
int
spin_holding(struct Spinlock *lock)
{
  int r;

  irq_save();
  r = lock->locked && (lock->cpu == my_cpu());
  irq_restore();

  return r;
}

// Record the current stack backtrace by following the frame pointer chain
static void
spin_get_caller_pcs(struct Spinlock *lock)
{
  uint32_t *fp;
  int i;

  fp = (uint32_t *) read_fp();
  for (i = 0; i < 10; i++) {
    if (fp == NULL)
      break;
    lock->pcs[i] = fp[-1];
    fp = (uint32_t *) fp[-3];
  }
  for ( ; i < 10; i++)
    lock->pcs[i] = 0;
}

static void
spin_print_caller_pcs(struct Spinlock *lock)
{
  struct PcDebugInfo info;
  uintptr_t pcs[10];
  int i;

  for (i = 0; i < 10; i++)
    pcs[i] = lock->pcs[i];

  for (i = 0; i < 10 && pcs[i]; i++) {
    debug_info_pc(pcs[i], &info);

    cprintf("  [%p] %s (%s at line %d)\n", pcs[i],
            info.fn_name, info.file, info.line);
  }
}
