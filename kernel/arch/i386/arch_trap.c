#include <signal.h>

#include <kernel/console.h>
#include <kernel/page.h>
#include <kernel/vmspace.h>
#include <kernel/process.h>
#include <kernel/sys.h>
#include <kernel/trap.h>
#include <kernel/types.h>
#include <kernel/core/irq.h>
#include <kernel/monitor.h>
#include <kernel/core/semaphore.h>
#include <kernel/interrupt.h>
#include <kernel/time.h>
#include <kernel/signal.h>

void
trap(struct TrapFrame *tf)
{
  // TODO
  (void) tf;
}

// Returns a human-readable name for the given trap number
// static const char *
// get_trap_name(unsigned trapno)
// {
//   // TODO
//   (void) trapno;
//   return NULL;
// }

void
print_trapframe(struct TrapFrame *tf)
{
  // TODO
  (void) tf;
}

int
ipi_irq(int, void *)
{
  // TODO
  return -1;
}

int
timer_irq(int, void *)
{
  // TODO
  return -1;
}

int 
arch_trap_frame_init(struct TrapFrame *tf, uintptr_t entry, uintptr_t arg1,
                     uintptr_t arg2, uintptr_t arg3, uintptr_t sp)
{
  // TODO
  (void) tf;
  (void) entry;
  (void) arg1;
  (void) arg2;
  (void) arg3;
  (void) sp;
  return -1;
}

void
arch_trap_frame_pop(struct TrapFrame *tf)
{
  // TODO:
  (void) tf;
}
