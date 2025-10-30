#include <kernel/console.h>
#include <kernel/kdebug.h>
#include <kernel/core/spinlock.h>
#include <kernel/types.h>

#include <arch/i386/regs.h>

static inline int
xchg(volatile int *p, int new_value)
{
	int old_value;

	asm volatile("lock\n\t"
               "xchgl %0, %1" :
			         "+m" (*p), "=a" (old_value) :
		           "1" (new_value) :
			         "cc");
	return old_value;
}

void
k_arch_spinlock_acquire(volatile int *locked)
{
  while (xchg(locked, 1) != 0)
    ;
}

void
k_arch_spinlock_release(volatile int *locked)
{
  xchg(locked, 0);
}

void
k_arch_spinlock_save_callstack(struct KSpinLock *spin)
{
  k_uintptr_t *ebp;
  int i;

  ebp = (k_uintptr_t *) ebp_get();

  for (i = 0; (ebp != NULL) && (i < K_SPINLOCK_MAX_PCS); i++) {
    spin->pcs[i] = ebp[1];
    ebp = (k_uintptr_t *) ebp[0];
  }

  for ( ; i < K_SPINLOCK_MAX_PCS; i++)
    spin->pcs[i] = 0;
}
