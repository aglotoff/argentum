#include <kernel/console.h>
#include <kernel/kdebug.h>
#include <kernel/spinlock.h>
#include <kernel/types.h>

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
  // FIXME: SMP
  (void) spin;
}

// static void
// print_info(uintptr_t pc)
// {
//   struct PcDebugInfo info;

//   debug_info_pc(pc, &info);
//   cprintf("  [%p] %s (%s at line %d)\n",
//           pc,
//           info.fn_name, info.file, info.line);
// }

// Display the recorded call stack along with debugging information
void
k_arch_spinlock_print_callstack(struct KSpinLock *spin)
{
  // FIXME: SMP
  (void) spin;
}
