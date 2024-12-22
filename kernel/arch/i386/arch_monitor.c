#include <stddef.h>


#include <kernel/monitor.h>
#include <kernel/trap.h>
#include <kernel/kdebug.h>
#include <kernel/console.h>

void
arch_mon_backtrace(struct TrapFrame *tf)
{
  // TODO
  (void) tf;
}
