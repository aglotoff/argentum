#include <kernel/console.h>
#include <kernel/kdebug.h>
#include <kernel/core/spinlock.h>
#include <kernel/types.h>

#include <arch/arm/regs.h>

// ARMv7-specific code to acquire a spinlock
void
k_arch_spinlock_acquire(volatile int *locked)
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
void
k_arch_spinlock_release(volatile int *locked)
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
void
k_arch_spinlock_save_callstack(struct KSpinLock *spin)
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

static void
print_info(uintptr_t pc)
{
  struct PcDebugInfo info;

  debug_info_pc(pc, &info);
  cprintf("  [%p] %s (%s at line %d)\n",
          pc,
          info.fn_name, info.file, info.line);
}

// Display the recorded call stack along with debugging information
void
k_arch_spinlock_print_callstack(struct KSpinLock *spin)
{
  int i;

  for (i = 0; i < SPIN_MAX_PCS && spin->pcs[i]; i++) {
    print_info(spin->pcs[i]);
  }
}
