#include <stddef.h>

#include <kernel/monitor.h>
#include <kernel/trap.h>
#include <kernel/kdebug.h>
#include <kernel/console.h>

#include <arch/i386/regs.h>

void
arch_mon_backtrace(struct TrapFrame *tf)
{
  struct PcDebugInfo info;
  uint32_t *ebp;

  ebp = (uint32_t *) (tf ? tf->ebp : ebp_get());
  for ( ; ebp != NULL; ebp = (uint32_t *) ebp[0]) {
    debug_info_pc(ebp[1], &info);

    cprintf("  [%p] %s (%s at line %d)\n",
            ebp[1], info.fn_name, info.file, info.line);
  }
}
