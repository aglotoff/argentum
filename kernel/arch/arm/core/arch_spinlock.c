#include <kernel/core/config.h>
#include <kernel/core/spinlock.h>

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
