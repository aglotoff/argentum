#include <kernel/core/config.h>
#include <kernel/core/spinlock.h>

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
