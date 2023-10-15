#include <regs.h>
#include <spinlock.h>

void
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

void
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

// Indices for the stack backtrace data structure represented as an array of
// uint32_t values. See ARM Procedure Call Standard for more details
// To generate this structure for all functions, the code needs to be compiled
// with the -mapcs-frame and -fno-omit-frame-pointer flags

/** Save code pointer (fp points here) */
#define APCS_FRAME_PC    0
/** Return link value */
#define APCS_FRAME_LINK  -1
/** Return sp value */
#define APCS_FRAME_SP    -2
/** Return fp value */
#define APCS_FRAME_FP    -3

// Record the current call stack by following the frame pointer chain.
// To properly generate stack backtrace structures, the code must be compiled
// with the -mapcs-frame and -fno-omit-frame-pointer flags
void
arch_spin_pcs_save(uintptr_t *pcs, size_t max)
{
  uint32_t *fp;
  size_t i;

  fp = (uint32_t *) r11_get();

  for (i = 0; (fp != NULL) && (i < max); i++) {
    pcs[i] = fp[APCS_FRAME_LINK];
    fp = (uint32_t *) fp[APCS_FRAME_FP];
  }

  for ( ; i < max; i++)
    pcs[i] = 0;
}
